/* $Id$ */
/** @file
 * PATM - The Patch Manager, all contexts.
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
#define LOG_GROUP LOG_GROUP_PATM
#include <VBox/patm.h>
#include <VBox/cpum.h>
#include <VBox/dis.h>
#include <VBox/disopcode.h>
#include <VBox/em.h>
#include <VBox/err.h>
#include <VBox/selm.h>
#include <VBox/mm.h>
#include "PATMInternal.h"
#include <VBox/vm.h>
#include "PATMA.h"

#include <VBox/log.h>
#include <iprt/assert.h>


/**
 * Load virtualized flags.
 *
 * This function is called from CPUMRawEnter(). It doesn't have to update the
 * IF and IOPL eflags bits, the caller will enforce those to set and 0 repectively.
 *
 * @param   pVM         VM handle.
 * @param   pCtxCore    The cpu context core.
 * @see     pg_raw
 */
PATMDECL(void) PATMRawEnter(PVM pVM, PCPUMCTXCORE pCtxCore)
{
    bool fPatchCode = PATMIsPatchGCAddr(pVM, (RTGCPTR)pCtxCore->eip);

    /*
     * Currently we don't bother to check whether PATM is enabled or not.
     * For all cases where it isn't, IOPL will be safe and IF will be set.
     */
    register uint32_t efl = pCtxCore->eflags.u32;
    CTXSUFF(pVM->patm.s.pGCState)->uVMFlags = efl & PATM_VIRTUAL_FLAGS_MASK;
    AssertMsg((efl & X86_EFL_IF) || PATMShouldUseRawMode(pVM, (RTGCPTR)pCtxCore->eip), ("X86_EFL_IF is clear and PATM is disabled! (eip=%VGv eflags=%08x fPATM=%d pPATMGC=%VGv-%VGv\n", pCtxCore->eip, pCtxCore->eflags.u32, PATMIsEnabled(pVM), pVM->patm.s.pPatchMemGC, pVM->patm.s.pPatchMemGC + pVM->patm.s.cbPatchMem));

    AssertReleaseMsg(CTXSUFF(pVM->patm.s.pGCState)->fPIF || fPatchCode, ("fPIF=%d eip=%VGv\n", CTXSUFF(pVM->patm.s.pGCState)->fPIF, pCtxCore->eip));

    efl &= ~PATM_VIRTUAL_FLAGS_MASK;
    efl |= X86_EFL_IF;
    pCtxCore->eflags.u32 = efl;

#ifdef IN_RING3
#ifdef PATM_EMULATE_SYSENTER
    PCPUMCTX pCtx;
    int      rc;

    /* Check if the sysenter handler has changed. */
    rc = CPUMQueryGuestCtxPtr(pVM, &pCtx);
    AssertRC(rc);
    if (   rc == VINF_SUCCESS
        && pCtx->SysEnter.cs  != 0
        && pCtx->SysEnter.eip != 0
       )
    {
        if (pVM->patm.s.pfnSysEnterGC != (RTGCPTR)pCtx->SysEnter.eip)
        {
            pVM->patm.s.pfnSysEnterPatchGC = 0;
            pVM->patm.s.pfnSysEnterGC = 0;

            Log2(("PATMRawEnter: installing sysenter patch for %VGv\n", pCtx->SysEnter.eip));
            pVM->patm.s.pfnSysEnterPatchGC = PATMR3QueryPatchGCPtr(pVM, pCtx->SysEnter.eip);
            if (pVM->patm.s.pfnSysEnterPatchGC == 0)
            {
                rc = PATMR3InstallPatch(pVM, pCtx->SysEnter.eip, PATMFL_SYSENTER | PATMFL_CODE32);
                if (rc == VINF_SUCCESS)
                {
                    pVM->patm.s.pfnSysEnterPatchGC  = PATMR3QueryPatchGCPtr(pVM, pCtx->SysEnter.eip);
                    pVM->patm.s.pfnSysEnterGC       = (RTGCPTR)pCtx->SysEnter.eip;
                    Assert(pVM->patm.s.pfnSysEnterPatchGC);
                }
            }
            else
                pVM->patm.s.pfnSysEnterGC = (RTGCPTR)pCtx->SysEnter.eip;
        }
    }
    else
    {
        pVM->patm.s.pfnSysEnterPatchGC = 0;
        pVM->patm.s.pfnSysEnterGC = 0;
    }
#endif
#endif
}


/**
 * Restores virtualized flags.
 *
 * This function is called from CPUMRawLeave(). It will update the eflags register.
 *
 ** @note Only here we are allowed to switch back to guest code (without a special reason such as a trap in patch code)!!
 *
 * @param   pVM         VM handle.
 * @param   pCtxCore    The cpu context core.
 * @param   rawRC       Raw mode return code
 * @see     @ref pg_raw
 */
PATMDECL(void) PATMRawLeave(PVM pVM, PCPUMCTXCORE pCtxCore, int rawRC)
{
    bool fPatchCode = PATMIsPatchGCAddr(pVM, (RTGCPTR)pCtxCore->eip);
    /*
     * We will only be called if PATMRawEnter was previously called.
     */
    register uint32_t efl = pCtxCore->eflags.u32;
    efl = (efl & ~PATM_VIRTUAL_FLAGS_MASK) | (CTXSUFF(pVM->patm.s.pGCState)->uVMFlags & PATM_VIRTUAL_FLAGS_MASK);
    pCtxCore->eflags.u32 = efl;
    CTXSUFF(pVM->patm.s.pGCState)->uVMFlags = X86_EFL_IF;

    AssertReleaseMsg((efl & X86_EFL_IF) || fPatchCode || rawRC == VINF_PATM_PENDING_IRQ_AFTER_IRET, ("Inconsistent state at %VGv\n", pCtxCore->eip));
    AssertReleaseMsg(CTXSUFF(pVM->patm.s.pGCState)->fPIF || fPatchCode, ("fPIF=%d eip=%VGv\n", CTXSUFF(pVM->patm.s.pGCState)->fPIF, pCtxCore->eip));

#ifdef IN_RING3
    if (    (efl & X86_EFL_IF)
        &&  fPatchCode
       )
    {
        if (    rawRC < VINF_PATM_LEAVEGC_FIRST
            ||  rawRC > VINF_PATM_LEAVEGC_LAST)
        {
            /*
             * Golden rules:
             * - Don't interrupt special patch streams that replace special instructions
             * - Don't break instruction fusing (sti, pop ss, mov ss)
             * - Don't go back to an instruction that has been overwritten by a patch jump
             * - Don't interrupt an idt handler on entry (1st instruction); technically incorrect
             *
             */
            if (CTXSUFF(pVM->patm.s.pGCState)->fPIF == 1)            /* consistent patch instruction state */
            {
                PATMTRANSSTATE  enmState;
                RTGCPTR         pOrgInstrGC = PATMR3PatchToGCPtr(pVM, pCtxCore->eip, &enmState);

                AssertRelease(pOrgInstrGC);

                Assert(enmState != PATMTRANS_OVERWRITTEN);
                if (enmState == PATMTRANS_SAFE)
                {
                    Assert(!PATMFindActivePatchByEntrypoint(pVM, pOrgInstrGC));
                    Log(("Switchback from %VGv to %VGv (Psp=%x)\n", pCtxCore->eip, pOrgInstrGC, CTXSUFF(pVM->patm.s.pGCState)->Psp));
                    STAM_COUNTER_INC(&pVM->patm.s.StatSwitchBack);
                    pCtxCore->eip = pOrgInstrGC;
                    fPatchCode = false; /* to reset the stack ptr */

                    CTXSUFF(pVM->patm.s.pGCState)->GCPtrInhibitInterrupts = 0;   /* reset this pointer; safe otherwise the state would be PATMTRANS_INHIBITIRQ */
                }
                else
                {
                    LogFlow(("Patch address %VGv can't be interrupted (state=%d)!\n",  pCtxCore->eip, enmState));
                    STAM_COUNTER_INC(&pVM->patm.s.StatSwitchBackFail);
                }
            }
            else
            {
                LogFlow(("Patch address %VGv can't be interrupted (fPIF=%d)!\n",  pCtxCore->eip, CTXSUFF(pVM->patm.s.pGCState)->fPIF));
                STAM_COUNTER_INC(&pVM->patm.s.StatSwitchBackFail);
            }
        }
    }
#else /* !IN_RING3 */
    AssertMsgFailed(("!IN_RING3"));
#endif  /* !IN_RING3 */

    if (!fPatchCode)
    {
        if (CTXSUFF(pVM->patm.s.pGCState)->GCPtrInhibitInterrupts == (RTGCPTR)pCtxCore->eip)
        {
            EMSetInhibitInterruptsPC(pVM, pCtxCore->eip);
        }
        CTXSUFF(pVM->patm.s.pGCState)->GCPtrInhibitInterrupts = 0;

        /* Reset the stack pointer to the top of the stack. */
#ifdef DEBUG
        if (CTXSUFF(pVM->patm.s.pGCState)->Psp != PATM_STACK_SIZE)
        {
            LogFlow(("PATMRawLeave: Reset PATM stack (Psp = %x)\n", CTXSUFF(pVM->patm.s.pGCState)->Psp));
        }
#endif
        CTXSUFF(pVM->patm.s.pGCState)->Psp = PATM_STACK_SIZE;
    }
}

/**
 * Get the EFLAGS.
 * This is a worker for CPUMRawGetEFlags().
 *
 * @returns The eflags.
 * @param   pVM         The VM handle.
 * @param   pCtxCore    The context core.
 */
PATMDECL(uint32_t) PATMRawGetEFlags(PVM pVM, PCCPUMCTXCORE pCtxCore)
{
    uint32_t efl = pCtxCore->eflags.u32;
    efl &= ~PATM_VIRTUAL_FLAGS_MASK;
    efl |= pVM->patm.s.CTXSUFF(pGCState)->uVMFlags & PATM_VIRTUAL_FLAGS_MASK;
    return efl;
}

/**
 * Updates the EFLAGS.
 * This is a worker for CPUMRawSetEFlags().
 *
 * @param   pVM         The VM handle.
 * @param   pCtxCore    The context core.
 * @param   efl         The new EFLAGS value.
 */
PATMDECL(void) PATMRawSetEFlags(PVM pVM, PCPUMCTXCORE pCtxCore, uint32_t efl)
{
    pVM->patm.s.CTXSUFF(pGCState)->uVMFlags = efl & PATM_VIRTUAL_FLAGS_MASK;
    efl &= ~PATM_VIRTUAL_FLAGS_MASK;
    efl |= X86_EFL_IF;
    pCtxCore->eflags.u32 = efl;
}

/**
 * Check if we must use raw mode (patch code being executed)
 *
 * @param   pVM         VM handle.
 * @param   pAddrGC     Guest context address
 */
PATMDECL(bool) PATMShouldUseRawMode(PVM pVM, RTGCPTR pAddrGC)
{
    return (    PATMIsEnabled(pVM)
            && ((pAddrGC >= pVM->patm.s.pPatchMemGC && pAddrGC < pVM->patm.s.pPatchMemGC + pVM->patm.s.cbPatchMem))) ? true : false;
}

/**
 * Returns the guest context pointer and size of the GC context structure
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
PATMDECL(GCPTRTYPE(PPATMGCSTATE)) PATMQueryGCState(PVM pVM)
{
    return pVM->patm.s.pGCStateGC;
}

/**
 * Checks whether the GC address is part of our patch region
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pAddrGC     Guest context address
 */
PATMDECL(bool) PATMIsPatchGCAddr(PVM pVM, RTGCPTR pAddrGC)
{
    return (PATMIsEnabled(pVM) && pAddrGC >= pVM->patm.s.pPatchMemGC && pAddrGC < pVM->patm.s.pPatchMemGC + pVM->patm.s.cbPatchMem) ? true : false;
}

/**
 * Set parameters for pending MMIO patch operation
 *
 * @returns VBox status code.
 * @param   pDevIns         Device instance.
 * @param   GCPhys          MMIO physical address
 * @param   pCachedData     GC pointer to cached data
 */
PATMDECL(int) PATMSetMMIOPatchInfo(PVM pVM, RTGCPHYS GCPhys, RTGCPTR pCachedData)
{
    pVM->patm.s.mmio.GCPhys = GCPhys;
    pVM->patm.s.mmio.pCachedData = pCachedData;

    return VINF_SUCCESS;
}

/**
 * Checks if the interrupt flag is enabled or not.
 *
 * @returns true if it's enabled.
 * @returns false if it's diabled.
 *
 * @param   pVM         The VM handle.
 */
PATMDECL(bool) PATMAreInterruptsEnabled(PVM pVM)
{
    PCPUMCTX pCtx = 0;
    int      rc;

    rc = CPUMQueryGuestCtxPtr(pVM, &pCtx);
    AssertRC(rc);

    return PATMAreInterruptsEnabledByCtxCore(pVM, CPUMCTX2CORE(pCtx));
}

/**
 * Checks if the interrupt flag is enabled or not.
 *
 * @returns true if it's enabled.
 * @returns false if it's diabled.
 *
 * @param   pVM         The VM handle.
 * @param   pCtxCore    CPU context
 */
PATMDECL(bool) PATMAreInterruptsEnabledByCtxCore(PVM pVM, PCPUMCTXCORE pCtxCore)
{
    if (PATMIsEnabled(pVM))
    {
        if (PATMIsPatchGCAddr(pVM, (RTGCPTR)pCtxCore->eip))
            return false;
    }
    return !!(pCtxCore->eflags.u32 & X86_EFL_IF);
}

/**
 * Check if the instruction is patched as a duplicated function
 *
 * @returns patch record
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    Guest context point to the instruction
 *
 */
PATMDECL(PPATMPATCHREC) PATMQueryFunctionPatch(PVM pVM, RTGCPTR pInstrGC)
{
    PPATMPATCHREC pRec;

    pRec = (PPATMPATCHREC)RTAvloGCPtrGet(&CTXSUFF(pVM->patm.s.PatchLookupTree)->PatchTree, pInstrGC);
    if (    pRec
        && (pRec->patch.uState == PATCH_ENABLED)
        && (pRec->patch.flags & (PATMFL_DUPLICATE_FUNCTION|PATMFL_CALLABLE_AS_FUNCTION))
       )
        return pRec;
    return 0;
}

/**
 * Checks if the int 3 was caused by a patched instruction
 *
 * @returns VBox status
 *
 * @param   pVM         The VM handle.
 * @param   pInstrGC    Instruction pointer
 * @param   pOpcode     Original instruction opcode (out, optional)
 * @param   pSize       Original instruction size (out, optional)
 */
PATMDECL(bool) PATMIsInt3Patch(PVM pVM, RTGCPTR pInstrGC, uint32_t *pOpcode, uint32_t *pSize)
{
    PPATMPATCHREC pRec;

    pRec = (PPATMPATCHREC)RTAvloGCPtrGet(&CTXSUFF(pVM->patm.s.PatchLookupTree)->PatchTree, pInstrGC);
    if (    pRec
        && (pRec->patch.uState == PATCH_ENABLED)
        && (pRec->patch.flags & (PATMFL_INT3_REPLACEMENT|PATMFL_INT3_REPLACEMENT_BLOCK))
       )
    {
        if (pOpcode) *pOpcode = pRec->patch.opcode;
        if (pSize)   *pSize   = pRec->patch.cbPrivInstr;
        return true;
    }
    return false;
}

/**
 * Emulate sysenter, sysexit and syscall instructions
 *
 * @returns VBox status
 *
 * @param   pVM         The VM handle.
 * @param   pCtxCore    The relevant core context.
 * @param   pCpu        Disassembly context
 */
PATMDECL(int) PATMSysCall(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu)
{
    PCPUMCTX pCtx;
    int      rc;

    rc = CPUMQueryGuestCtxPtr(pVM, &pCtx);
    AssertRCReturn(rc, VINF_EM_RAW_RING_SWITCH);

    if (pCpu->pCurInstr->opcode == OP_SYSENTER)
    {
        if (    pCtx->SysEnter.cs == 0
            ||  (pRegFrame->cs & X86_SEL_RPL) != 3
            ||  pVM->patm.s.pfnSysEnterPatchGC == 0
            ||  pVM->patm.s.pfnSysEnterGC != (RTGCPTR)pCtx->SysEnter.eip
            ||  !(PATMRawGetEFlags(pVM, pRegFrame) & X86_EFL_IF))
            goto end;

        Log2(("PATMSysCall: sysenter from %VGv to %VGv\n", pRegFrame->eip, pVM->patm.s.pfnSysEnterPatchGC));
        /** @todo the base and limit are forced to 0 & 4G-1 resp. We assume the selector is wide open here. */
        /** @note The Intel manual suggests that the OS is responsible for this. */
        pRegFrame->cs          = (pCtx->SysEnter.cs & ~X86_SEL_RPL) | 1;
        pRegFrame->eip         = /** @todo ugly conversion! */(uint32_t)pVM->patm.s.pfnSysEnterPatchGC;
        pRegFrame->ss          = pRegFrame->cs + 8;     /* SysEnter.cs + 8 */
        pRegFrame->esp         = pCtx->SysEnter.esp;
        pRegFrame->eflags.u32 &= ~(X86_EFL_VM|X86_EFL_RF);
        pRegFrame->eflags.u32 |= X86_EFL_IF;

        /* Turn off interrupts. */
        pVM->patm.s.CTXSUFF(pGCState)->uVMFlags &= ~X86_EFL_IF;

        STAM_COUNTER_INC(&pVM->patm.s.StatSysEnter);

        return VINF_SUCCESS;
    }
    else
    if (pCpu->pCurInstr->opcode == OP_SYSEXIT)
    {
        if (    pCtx->SysEnter.cs == 0
            ||  (pRegFrame->cs & X86_SEL_RPL) != 1
            ||  !(PATMRawGetEFlags(pVM, pRegFrame) & X86_EFL_IF))
            goto end;

        Log2(("PATMSysCall: sysexit from %VGv to %VGv\n", pRegFrame->eip, pRegFrame->edx));

        pRegFrame->cs          = ((pCtx->SysEnter.cs + 16) & ~X86_SEL_RPL) | 3;
        pRegFrame->eip         = pRegFrame->edx;
        pRegFrame->ss          = pRegFrame->cs + 8;  /* SysEnter.cs + 24 */
        pRegFrame->esp         = pRegFrame->ecx;

        STAM_COUNTER_INC(&pVM->patm.s.StatSysExit);

        return VINF_SUCCESS;
    }
    else
    if (pCpu->pCurInstr->opcode == OP_SYSCALL)
    {
        /** @todo implement syscall */
    }
    else
    if (pCpu->pCurInstr->opcode == OP_SYSRET)
    {
        /** @todo implement sysret */
    }

end:
    return VINF_EM_RAW_RING_SWITCH;
}

/**
 * Checks if the illegal instruction was caused by a patched instruction
 *
 * @returns VBox status
 *
 * @param   pVM         The VM handle.
 * @param   pCtxCore    The relevant core context.
 */
PATMDECL(int) PATMHandleIllegalInstrTrap(PVM pVM, PCPUMCTXCORE pRegFrame)
{
    PPATMPATCHREC pRec;
    int rc;

    /* Very important check -> otherwise we have a security leak. */
    AssertReturn((pRegFrame->ss & X86_SEL_RPL) == 1, VERR_ACCESS_DENIED);
    Assert(PATMIsPatchGCAddr(pVM, (RTGCPTR)pRegFrame->eip));

    /* OP_ILLUD2 in PATM generated code? */
    if (CTXSUFF(pVM->patm.s.pGCState)->uPendingAction)
    {
        LogFlow(("PATMGC: Pending action %x at %VGv\n", CTXSUFF(pVM->patm.s.pGCState)->uPendingAction, pRegFrame->eip));

        /* Private PATM interface (@todo hack due to lack of anything generic). */
        /* Parameters:
         *  eax = Pending action (currently PATM_ACTION_LOOKUP_ADDRESS)
         *  ecx = PATM_ACTION_MAGIC
         */
        if (    (pRegFrame->eax & CTXSUFF(pVM->patm.s.pGCState)->uPendingAction)
            &&   pRegFrame->ecx == PATM_ACTION_MAGIC
           )
        {
            CTXSUFF(pVM->patm.s.pGCState)->uPendingAction = 0;

            switch (pRegFrame->eax)
            {
            case PATM_ACTION_LOOKUP_ADDRESS:
            {
                /* Parameters:
                 *  edx = GC address to find
                 *  edi = PATCHJUMPTABLE ptr
                 */
                AssertMsg(!pRegFrame->edi || PATMIsPatchGCAddr(pVM, (RTGCPTR)pRegFrame->edi), ("edx = %VGv\n", pRegFrame->edi));

                Log(("PATMGC: lookup %VGv jump table=%VGv\n", pRegFrame->edx, pRegFrame->edi));

                pRec = PATMQueryFunctionPatch(pVM, (RTGCPTR)(pRegFrame->edx));
                if (pRec)
                {
                    if (pRec->patch.uState == PATCH_ENABLED)
                    {
                        RTGCUINTPTR pRelAddr = pRec->patch.pPatchBlockOffset;   /* make it relative */
                        rc = PATMAddBranchToLookupCache(pVM, (RTGCPTR)pRegFrame->edi, (RTGCPTR)pRegFrame->edx, pRelAddr);
                        if (rc == VINF_SUCCESS)
                        {
                            pRegFrame->eip += PATM_ILLEGAL_INSTR_SIZE;
                            pRegFrame->eax = pRelAddr;
                            STAM_COUNTER_INC(&pVM->patm.s.StatFunctionFound);
                            return VINF_SUCCESS;
                        }
                        AssertFailed();
                    }
                    else
                    {
                        pRegFrame->eip += PATM_ILLEGAL_INSTR_SIZE;
                        pRegFrame->eax = 0;     /* make it fault */
                        STAM_COUNTER_INC(&pVM->patm.s.StatFunctionNotFound);
                        return VINF_SUCCESS;
                    }
                }
                else
                {
#if 0
                    if (pRegFrame->edx == 0x806eca98) 
                    {
                        pRegFrame->eip += PATM_ILLEGAL_INSTR_SIZE;
                        pRegFrame->eax = 0;     /* make it fault */
                        STAM_COUNTER_INC(&pVM->patm.s.StatFunctionNotFound);
                        return VINF_SUCCESS;
                    } 
#endif
                    STAM_COUNTER_INC(&pVM->patm.s.StatFunctionNotFound);
                    return VINF_PATM_DUPLICATE_FUNCTION;
                }
            }

            case PATM_ACTION_DISPATCH_PENDING_IRQ:
                /* Parameters:
                 *  edi = GC address to jump to
                 */
                Log(("PATMGC: Dispatch pending interrupt; eip=%VGv->%VGv\n", pRegFrame->eip, pRegFrame->edi));

                /* Change EIP to the guest address the patch would normally jump to after setting IF. */
                pRegFrame->eip = pRegFrame->edi;

                Assert(pVM->patm.s.CTXSUFF(pGCState)->Restore.uFlags == (PATM_RESTORE_EAX|PATM_RESTORE_ECX|PATM_RESTORE_EDI));
                Assert(pVM->patm.s.CTXSUFF(pGCState)->fPIF == 0);

                pRegFrame->eax = pVM->patm.s.CTXSUFF(pGCState)->Restore.uEAX;
                pRegFrame->ecx = pVM->patm.s.CTXSUFF(pGCState)->Restore.uECX;
                pRegFrame->edi = pVM->patm.s.CTXSUFF(pGCState)->Restore.uEDI;

                pVM->patm.s.CTXSUFF(pGCState)->Restore.uFlags = 0;

                /* We are no longer executing PATM code; set PIF again. */
                pVM->patm.s.CTXSUFF(pGCState)->fPIF = 1;

                STAM_COUNTER_INC(&pVM->patm.s.StatCheckPendingIRQ);

                /* The caller will call trpmGCExitTrap, which will dispatch pending interrupts for us. */
                return VINF_SUCCESS;

            case PATM_ACTION_PENDING_IRQ_AFTER_IRET:
                /* Parameters:
                 *  edi = GC address to jump to
                 */
                Log(("PATMGC: Dispatch pending interrupt (iret); eip=%VGv->%VGv\n", pRegFrame->eip, pRegFrame->edi));
                Assert(pVM->patm.s.CTXSUFF(pGCState)->Restore.uFlags == (PATM_RESTORE_EAX|PATM_RESTORE_ECX|PATM_RESTORE_EDI));
                Assert(pVM->patm.s.CTXSUFF(pGCState)->fPIF == 0);

                /* Change EIP to the guest address of the iret. */
                pRegFrame->eip = pRegFrame->edi;

                pRegFrame->eax = pVM->patm.s.CTXSUFF(pGCState)->Restore.uEAX;
                pRegFrame->ecx = pVM->patm.s.CTXSUFF(pGCState)->Restore.uECX;
                pRegFrame->edi = pVM->patm.s.CTXSUFF(pGCState)->Restore.uEDI;
                pVM->patm.s.CTXSUFF(pGCState)->Restore.uFlags = 0;

                /* We are no longer executing PATM code; set PIF again. */
                pVM->patm.s.CTXSUFF(pGCState)->fPIF = 1;

                return VINF_PATM_PENDING_IRQ_AFTER_IRET;

            case PATM_ACTION_DO_V86_IRET:
            {
                Log(("PATMGC: Do iret to V86 code; eip=%VGv\n", pRegFrame->eip));
                Assert(pVM->patm.s.CTXSUFF(pGCState)->Restore.uFlags == (PATM_RESTORE_EAX|PATM_RESTORE_ECX));
                Assert(pVM->patm.s.CTXSUFF(pGCState)->fPIF == 0);

                pRegFrame->eax = pVM->patm.s.CTXSUFF(pGCState)->Restore.uEAX;
                pRegFrame->ecx = pVM->patm.s.CTXSUFF(pGCState)->Restore.uECX;
                pVM->patm.s.CTXSUFF(pGCState)->Restore.uFlags = 0;

                rc = EMInterpretIret(pVM, pRegFrame);
                if (VBOX_SUCCESS(rc))
                {
                    STAM_COUNTER_INC(&pVM->patm.s.StatEmulIret); 

                    /* We are no longer executing PATM code; set PIF again. */
                    pVM->patm.s.CTXSUFF(pGCState)->fPIF = 1;
                }
                else 
                    STAM_COUNTER_INC(&pVM->patm.s.StatEmulIretFailed); 
                return rc;
            }

#ifdef DEBUG
            case PATM_ACTION_LOG_CLI:
                Log(("PATMGC: CLI at %VGv (current IF=%d iopl=%d)\n", pRegFrame->eip, !!(pVM->patm.s.CTXSUFF(pGCState)->uVMFlags & X86_EFL_IF), X86_EFL_GET_IOPL(pVM->patm.s.CTXSUFF(pGCState)->uVMFlags) ));
                pRegFrame->eip += PATM_ILLEGAL_INSTR_SIZE;
                return VINF_SUCCESS;

            case PATM_ACTION_LOG_STI:
                Log(("PATMGC: STI at %VGv (current IF=%d iopl=%d)\n", pRegFrame->eip, !!(pVM->patm.s.CTXSUFF(pGCState)->uVMFlags & X86_EFL_IF), X86_EFL_GET_IOPL(pVM->patm.s.CTXSUFF(pGCState)->uVMFlags) ));
                pRegFrame->eip += PATM_ILLEGAL_INSTR_SIZE;
                return VINF_SUCCESS;

            case PATM_ACTION_LOG_POPF_IF1:
                Log(("PATMGC: POPF setting IF at %VGv (current IF=%d iopl=%d)\n", pRegFrame->eip, !!(pVM->patm.s.CTXSUFF(pGCState)->uVMFlags & X86_EFL_IF), X86_EFL_GET_IOPL(pVM->patm.s.CTXSUFF(pGCState)->uVMFlags)));
                pRegFrame->eip += PATM_ILLEGAL_INSTR_SIZE;
                return VINF_SUCCESS;

            case PATM_ACTION_LOG_POPF_IF0:
                Log(("PATMGC: POPF at %VGv (current IF=%d iopl=%d)\n", pRegFrame->eip, !!(pVM->patm.s.CTXSUFF(pGCState)->uVMFlags & X86_EFL_IF), X86_EFL_GET_IOPL(pVM->patm.s.CTXSUFF(pGCState)->uVMFlags)));
                pRegFrame->eip += PATM_ILLEGAL_INSTR_SIZE;
                return VINF_SUCCESS;

            case PATM_ACTION_LOG_PUSHF:
                Log(("PATMGC: PUSHF at %VGv (current IF=%d iopl=%d)\n", pRegFrame->eip, !!(pVM->patm.s.CTXSUFF(pGCState)->uVMFlags & X86_EFL_IF), X86_EFL_GET_IOPL(pVM->patm.s.CTXSUFF(pGCState)->uVMFlags) ));
                pRegFrame->eip += PATM_ILLEGAL_INSTR_SIZE;
                return VINF_SUCCESS;

            case PATM_ACTION_LOG_IF1:
                Log(("PATMGC: IF=1 escape from %VGv\n", pRegFrame->eip));
                pRegFrame->eip += PATM_ILLEGAL_INSTR_SIZE;
                return VINF_SUCCESS;

            case PATM_ACTION_LOG_IRET:
            {
#ifdef IN_GC
                char    *pIretFrame = (char *)pRegFrame->edx;
                uint32_t eip, selCS, uEFlags;

                rc  = MMGCRamRead(pVM, &eip,     pIretFrame, 4);
                rc |= MMGCRamRead(pVM, &selCS,   pIretFrame + 4, 4);
                rc |= MMGCRamRead(pVM, &uEFlags, pIretFrame + 8, 4);
                if (rc == VINF_SUCCESS)
                {
                    if (    (uEFlags & X86_EFL_VM)
                        ||  (selCS & X86_SEL_RPL) == 3)
                    {
                        uint32_t selSS, esp;

                        rc |= MMGCRamRead(pVM, &esp,     pIretFrame + 12, 4);
                        rc |= MMGCRamRead(pVM, &selSS,   pIretFrame + 16, 4);

                        if (uEFlags & X86_EFL_VM)
                        {
                            uint32_t selDS, selES, selFS, selGS;
                            rc  = MMGCRamRead(pVM, &selES,   pIretFrame + 20, 4);
                            rc |= MMGCRamRead(pVM, &selDS,   pIretFrame + 24, 4);
                            rc |= MMGCRamRead(pVM, &selFS,   pIretFrame + 28, 4);
                            rc |= MMGCRamRead(pVM, &selGS,   pIretFrame + 32, 4);
                            if (rc == VINF_SUCCESS)
                            {
                                Log(("PATMGC: IRET->VM stack frame: return address %04X:%VGv eflags=%08x ss:esp=%04X:%VGv\n", selCS, eip, uEFlags, selSS, esp));
                                Log(("PATMGC: IRET->VM stack frame: DS=%04X ES=%04X FS=%04X GS=%04X\n", selDS, selES, selFS, selGS));
                            }
                        }
                        else
                            Log(("PATMGC: IRET stack frame: return address %04X:%VGv eflags=%08x ss:esp=%04X:%VGv\n", selCS, eip, uEFlags, selSS, esp));
                    }
                    else
                        Log(("PATMGC: IRET stack frame: return address %04X:%VGv eflags=%08x\n", selCS, eip, uEFlags));
                }
#endif
                Log(("PATMGC: IRET from %VGv (IF->1) current eflags=%x\n", pRegFrame->eip, pVM->patm.s.CTXSUFF(pGCState)->uVMFlags));
                pRegFrame->eip += PATM_ILLEGAL_INSTR_SIZE;
                return VINF_SUCCESS;
            }

            case PATM_ACTION_LOG_RET:
                Log(("PATMGC: RET to %VGv ESP=%VGv iopl=%d\n", pRegFrame->edx, pRegFrame->ebx, X86_EFL_GET_IOPL(pVM->patm.s.CTXSUFF(pGCState)->uVMFlags)));
                pRegFrame->eip += PATM_ILLEGAL_INSTR_SIZE;
                return VINF_SUCCESS;

            case PATM_ACTION_LOG_CALL:
                Log(("PATMGC: CALL to %VGv return addr %VGv ESP=%VGv iopl=%d\n", pVM->patm.s.CTXSUFF(pGCState)->GCCallPatchTargetAddr, pVM->patm.s.CTXSUFF(pGCState)->GCCallReturnAddr, pRegFrame->edx, X86_EFL_GET_IOPL(pVM->patm.s.CTXSUFF(pGCState)->uVMFlags)));
                pRegFrame->eip += PATM_ILLEGAL_INSTR_SIZE;
                return VINF_SUCCESS;
#endif
            default:
                AssertFailed();
                break;
            }
        }
        else
            AssertFailed();
        CTXSUFF(pVM->patm.s.pGCState)->uPendingAction = 0;
    }
    AssertMsgFailed(("Unexpected OP_ILLUD2 in patch code at %VGv (pending action %x)!!!!\n", pRegFrame->eip, CTXSUFF(pVM->patm.s.pGCState)->uPendingAction));
    return VINF_EM_RAW_EMULATE_INSTR;
}

/**
 * Checks if the int 3 was caused by a patched instruction
 *
 * @returns VBox status
 *
 * @param   pVM         The VM handle.
 * @param   pCtxCore    The relevant core context.
 */
PATMDECL(int) PATMHandleInt3PatchTrap(PVM pVM, PCPUMCTXCORE pRegFrame)
{
    PPATMPATCHREC pRec;
    int rc;

    Assert((pRegFrame->ss & X86_SEL_RPL) == 1);

    /* Int 3 in PATM generated code? (most common case) */
    if (PATMIsPatchGCAddr(pVM, (RTGCPTR)pRegFrame->eip))
    {
        /* @note hardcoded assumption about it being a single byte int 3 instruction. */
        pRegFrame->eip--;
        return VINF_PATM_PATCH_INT3;
    }

    /** @todo could use simple caching here to speed things up. */
    pRec = (PPATMPATCHREC)RTAvloGCPtrGet(&CTXSUFF(pVM->patm.s.PatchLookupTree)->PatchTree, (RTGCPTR)(pRegFrame->eip - 1));  /* eip is pointing to the instruction *after* 'int 3' already */
    if (pRec && pRec->patch.uState == PATCH_ENABLED)
    {
        if (pRec->patch.flags & PATMFL_INT3_REPLACEMENT_BLOCK)
        {
            Assert(pRec->patch.opcode == OP_CLI);
            /* This is a special cli block that was turned into an int 3 patch. We jump to the generated code manually. */
            pRegFrame->eip = (uint32_t)PATCHCODE_PTR_GC(&pRec->patch);
            STAM_COUNTER_INC(&pVM->patm.s.StatInt3BlockRun);
            return VINF_SUCCESS;
        }
        else
        if (pRec->patch.flags & PATMFL_INT3_REPLACEMENT)
        {
            uint32_t    size, cbOp;
            DISCPUSTATE cpu;

            /* eip is pointing to the instruction *after* 'int 3' already */
            pRegFrame->eip = pRegFrame->eip - 1;

            PATM_STAT_RUN_INC(&pRec->patch);

            Log(("PATMHandleInt3PatchTrap found int3 for %s at %VGv\n", patmGetInstructionString(pRec->patch.opcode, 0), pRegFrame->eip));

            switch(pRec->patch.opcode)
            {
            case OP_CPUID:
            case OP_IRET:
                break;

            case OP_STR:
            case OP_SGDT:
            case OP_SLDT:
            case OP_SIDT:
            case OP_LSL:
            case OP_LAR:
            case OP_SMSW:
            case OP_VERW:
            case OP_VERR:
            default:
                PATM_STAT_FAULT_INC(&pRec->patch);
                pRec->patch.cTraps++;
                return VINF_EM_RAW_EMULATE_INSTR;
            }

            cpu.mode = SELMIsSelector32Bit(pVM, pRegFrame->cs, 0) ? CPUMODE_32BIT : CPUMODE_16BIT;
            if(cpu.mode != CPUMODE_32BIT)
            {
                AssertFailed();
                return VINF_EM_RAW_EMULATE_INSTR;
            }
            rc = DISCoreOne(&cpu, (RTUINTPTR)&pRec->patch.aPrivInstr[0], &cbOp);
            if (VBOX_FAILURE(rc))
            {
                Log(("DISCoreOne failed with %Vrc\n", rc));
                PATM_STAT_FAULT_INC(&pRec->patch);
                pRec->patch.cTraps++;
                return VINF_EM_RAW_EMULATE_INSTR;
            }

            rc = EMInterpretInstructionCPU(pVM, &cpu, pRegFrame, 0 /* not relevant here */, &size);
            if (rc != VINF_SUCCESS)
            {
                Log(("EMInterpretInstructionCPU failed with %Vrc\n", rc));
                PATM_STAT_FAULT_INC(&pRec->patch);
                pRec->patch.cTraps++;
                return VINF_EM_RAW_EMULATE_INSTR;
            }

            pRegFrame->eip += cpu.opsize;
            return VINF_SUCCESS;
        }
    }
    return VERR_PATCH_NOT_FOUND;
}

/**
 * Adds branch pair to the lookup cache of the particular branch instruction
 *
 * @returns VBox status
 * @param   pVM                 The VM to operate on.
 * @param   pJumpTableGC        Pointer to branch instruction lookup cache
 * @param   pBranchTarget       Original branch target
 * @param   pRelBranchPatch     Relative duplicated function address
 */
int PATMAddBranchToLookupCache(PVM pVM, RTGCPTR pJumpTableGC, RTGCPTR pBranchTarget, RTGCUINTPTR pRelBranchPatch)
{
    PPATCHJUMPTABLE pJumpTable;

    Log(("PATMAddBranchToLookupCache: Adding (%VGv->%VGv (%VGv)) to table %VGv\n", pBranchTarget, pRelBranchPatch + pVM->patm.s.pPatchMemGC, pRelBranchPatch, pJumpTableGC));

    AssertReturn(PATMIsPatchGCAddr(pVM, pJumpTableGC), VERR_INVALID_PARAMETER);

#ifdef IN_GC
    pJumpTable = (PPATCHJUMPTABLE) pJumpTableGC;
#else
    pJumpTable = (PPATCHJUMPTABLE) (pJumpTableGC - pVM->patm.s.pPatchMemGC + pVM->patm.s.pPatchMemHC);
#endif
    Log(("Nr addresses = %d, insert pos = %d\n", pJumpTable->cAddresses, pJumpTable->ulInsertPos));
    if (pJumpTable->cAddresses < pJumpTable->nrSlots)
    {
        uint32_t i;

        for (i=0;i<pJumpTable->nrSlots;i++)
        {
            if (pJumpTable->Slot[i].pInstrGC == 0)
            {
                pJumpTable->Slot[i].pInstrGC    = pBranchTarget;
                /* Relative address - eases relocation */
                pJumpTable->Slot[i].pRelPatchGC = pRelBranchPatch;
                pJumpTable->cAddresses++;
                break;
            }
        }
        AssertReturn(i < pJumpTable->nrSlots, VERR_INTERNAL_ERROR);
#ifdef VBOX_WITH_STATISTICS
        STAM_COUNTER_INC(&pVM->patm.s.StatFunctionLookupInsert);
        if (pVM->patm.s.StatU32FunctionMaxSlotsUsed < i)
            pVM->patm.s.StatU32FunctionMaxSlotsUsed = i + 1;
#endif
    }
    else
    {
        /* Replace an old entry. */
        /** @todo replacement strategy isn't really bright. change to something better if required. */
        Assert(pJumpTable->ulInsertPos < pJumpTable->nrSlots);
        Assert((pJumpTable->nrSlots & 1) == 0);

        pJumpTable->ulInsertPos &= (pJumpTable->nrSlots-1);
        pJumpTable->Slot[pJumpTable->ulInsertPos].pInstrGC    = pBranchTarget;
        /* Relative address - eases relocation */
        pJumpTable->Slot[pJumpTable->ulInsertPos].pRelPatchGC = pRelBranchPatch;

        pJumpTable->ulInsertPos = (pJumpTable->ulInsertPos+1) & (pJumpTable->nrSlots-1);

        STAM_COUNTER_INC(&pVM->patm.s.StatFunctionLookupReplace);
    }

    return VINF_SUCCESS;
}

/**
 * Return the name of the patched instruction
 *
 * @returns instruction name
 *
 * @param   opcode      DIS instruction opcode
 * @param   fPatchFlags Patch flags
 */
PATMDECL(char *)patmGetInstructionString(uint32_t opcode, uint32_t fPatchFlags)
{
    char *pszInstr = NULL;

    switch (opcode)
    {
    case OP_CLI:
        pszInstr = "cli";
        break;
    case OP_PUSHF:
        pszInstr = "pushf";
        break;
    case OP_POPF:
        pszInstr = "popf";
        break;
    case OP_STR:
        pszInstr = "str";
        break;
    case OP_LSL:
        pszInstr = "lsl";
        break;
    case OP_LAR:
        pszInstr = "lar";
        break;
    case OP_SGDT:
        pszInstr = "sgdt";
        break;
    case OP_SLDT:
        pszInstr = "sldt";
        break;
    case OP_SIDT:
        pszInstr = "sidt";
        break;
    case OP_SMSW:
        pszInstr = "smsw";
        break;
    case OP_VERW:
        pszInstr = "verw";
        break;
    case OP_VERR:
        pszInstr = "verr";
        break;
    case OP_CPUID:
        pszInstr = "cpuid";
        break;
    case OP_JMP:
        pszInstr = "jmp";
        break;
    case OP_JO:
        pszInstr = "jo";
        break;
    case OP_JNO:
        pszInstr = "jno";
        break;
    case OP_JC:
        pszInstr = "jc";
        break;
    case OP_JNC:
        pszInstr = "jnc";
        break;
    case OP_JE:
        pszInstr = "je";
        break;
    case OP_JNE:
        pszInstr = "jne";
        break;
    case OP_JBE:
        pszInstr = "jbe";
        break;
    case OP_JNBE:
        pszInstr = "jnbe";
        break;
    case OP_JS:
        pszInstr = "js";
        break;
    case OP_JNS:
        pszInstr = "jns";
        break;
    case OP_JP:
        pszInstr = "jp";
        break;
    case OP_JNP:
        pszInstr = "jnp";
        break;
    case OP_JL:
        pszInstr = "jl";
        break;
    case OP_JNL:
        pszInstr = "jnl";
        break;
    case OP_JLE:
        pszInstr = "jle";
        break;
    case OP_JNLE:
        pszInstr = "jnle";
        break;
    case OP_JECXZ:
        pszInstr = "jecxz";
        break;
    case OP_LOOP:
        pszInstr = "loop";
        break;
    case OP_LOOPNE:
        pszInstr = "loopne";
        break;
    case OP_LOOPE:
        pszInstr = "loope";
        break;
    case OP_MOV:
        if (fPatchFlags & PATMFL_IDTHANDLER)
        {
            pszInstr = "mov (Int/Trap Handler)";
        }
        break;
    case OP_SYSENTER:
        pszInstr = "sysenter";
        break;
    case OP_PUSH:
        pszInstr = "push (cs)";
        break;
    case OP_CALL:
        pszInstr = "call";
        break;
    case OP_IRET:
        pszInstr = "iret";
        break;
    }
    return pszInstr;
}
