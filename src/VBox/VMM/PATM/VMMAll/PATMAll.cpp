/* $Id$ */
/** @file
 * PATM - The Patch Manager, all contexts.
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
#include <VBox/vmm.h>
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
VMMDECL(void) PATMRawEnter(PVM pVM, PCPUMCTXCORE pCtxCore)
{
    bool fPatchCode = PATMIsPatchGCAddr(pVM, (RTRCPTR)pCtxCore->eip);

    /*
     * Currently we don't bother to check whether PATM is enabled or not.
     * For all cases where it isn't, IOPL will be safe and IF will be set.
     */
    register uint32_t efl = pCtxCore->eflags.u32;
    CTXSUFF(pVM->patm.s.pGCState)->uVMFlags = efl & PATM_VIRTUAL_FLAGS_MASK;
    AssertMsg((efl & X86_EFL_IF) || PATMShouldUseRawMode(pVM, (RTRCPTR)pCtxCore->eip), ("X86_EFL_IF is clear and PATM is disabled! (eip=%RRv eflags=%08x fPATM=%d pPATMGC=%RRv-%RRv\n", pCtxCore->eip, pCtxCore->eflags.u32, PATMIsEnabled(pVM), pVM->patm.s.pPatchMemGC, pVM->patm.s.pPatchMemGC + pVM->patm.s.cbPatchMem));

    AssertReleaseMsg(CTXSUFF(pVM->patm.s.pGCState)->fPIF || fPatchCode, ("fPIF=%d eip=%RRv\n", CTXSUFF(pVM->patm.s.pGCState)->fPIF, pCtxCore->eip));

    efl &= ~PATM_VIRTUAL_FLAGS_MASK;
    efl |= X86_EFL_IF;
    pCtxCore->eflags.u32 = efl;

#ifdef IN_RING3
#ifdef PATM_EMULATE_SYSENTER
    PCPUMCTX pCtx;

    /* Check if the sysenter handler has changed. */
    pCtx = CPUMQueryGuestCtxPtr(pVM);
    if (   pCtx->SysEnter.cs  != 0
        && pCtx->SysEnter.eip != 0
       )
    {
        if (pVM->patm.s.pfnSysEnterGC != (RTRCPTR)pCtx->SysEnter.eip)
        {
            pVM->patm.s.pfnSysEnterPatchGC = 0;
            pVM->patm.s.pfnSysEnterGC = 0;

            Log2(("PATMRawEnter: installing sysenter patch for %RRv\n", pCtx->SysEnter.eip));
            pVM->patm.s.pfnSysEnterPatchGC = PATMR3QueryPatchGCPtr(pVM, pCtx->SysEnter.eip);
            if (pVM->patm.s.pfnSysEnterPatchGC == 0)
            {
                rc = PATMR3InstallPatch(pVM, pCtx->SysEnter.eip, PATMFL_SYSENTER | PATMFL_CODE32);
                if (rc == VINF_SUCCESS)
                {
                    pVM->patm.s.pfnSysEnterPatchGC  = PATMR3QueryPatchGCPtr(pVM, pCtx->SysEnter.eip);
                    pVM->patm.s.pfnSysEnterGC       = (RTRCPTR)pCtx->SysEnter.eip;
                    Assert(pVM->patm.s.pfnSysEnterPatchGC);
                }
            }
            else
                pVM->patm.s.pfnSysEnterGC = (RTRCPTR)pCtx->SysEnter.eip;
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
VMMDECL(void) PATMRawLeave(PVM pVM, PCPUMCTXCORE pCtxCore, int rawRC)
{
    bool fPatchCode = PATMIsPatchGCAddr(pVM, (RTRCPTR)pCtxCore->eip);
    /*
     * We will only be called if PATMRawEnter was previously called.
     */
    register uint32_t efl = pCtxCore->eflags.u32;
    efl = (efl & ~PATM_VIRTUAL_FLAGS_MASK) | (CTXSUFF(pVM->patm.s.pGCState)->uVMFlags & PATM_VIRTUAL_FLAGS_MASK);
    pCtxCore->eflags.u32 = efl;
    CTXSUFF(pVM->patm.s.pGCState)->uVMFlags = X86_EFL_IF;

    AssertReleaseMsg((efl & X86_EFL_IF) || fPatchCode || rawRC == VINF_PATM_PENDING_IRQ_AFTER_IRET || RT_FAILURE(rawRC), ("Inconsistent state at %RRv rc=%Rrc\n", pCtxCore->eip, rawRC));
    AssertReleaseMsg(CTXSUFF(pVM->patm.s.pGCState)->fPIF || fPatchCode || RT_FAILURE(rawRC), ("fPIF=%d eip=%RRv rc=%Rrc\n", CTXSUFF(pVM->patm.s.pGCState)->fPIF, pCtxCore->eip, rawRC));

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
                RTRCPTR         pOrgInstrGC = PATMR3PatchToGCPtr(pVM, pCtxCore->eip, &enmState);

                AssertRelease(pOrgInstrGC);

                Assert(enmState != PATMTRANS_OVERWRITTEN);
                if (enmState == PATMTRANS_SAFE)
                {
                    Assert(!PATMFindActivePatchByEntrypoint(pVM, pOrgInstrGC));
                    Log(("Switchback from %RRv to %RRv (Psp=%x)\n", pCtxCore->eip, pOrgInstrGC, CTXSUFF(pVM->patm.s.pGCState)->Psp));
                    STAM_COUNTER_INC(&pVM->patm.s.StatSwitchBack);
                    pCtxCore->eip = pOrgInstrGC;
                    fPatchCode = false; /* to reset the stack ptr */

                    CTXSUFF(pVM->patm.s.pGCState)->GCPtrInhibitInterrupts = 0;   /* reset this pointer; safe otherwise the state would be PATMTRANS_INHIBITIRQ */
                }
                else
                {
                    LogFlow(("Patch address %RRv can't be interrupted (state=%d)!\n",  pCtxCore->eip, enmState));
                    STAM_COUNTER_INC(&pVM->patm.s.StatSwitchBackFail);
                }
            }
            else
            {
                LogFlow(("Patch address %RRv can't be interrupted (fPIF=%d)!\n",  pCtxCore->eip, CTXSUFF(pVM->patm.s.pGCState)->fPIF));
                STAM_COUNTER_INC(&pVM->patm.s.StatSwitchBackFail);
            }
        }
    }
#else /* !IN_RING3 */
    AssertMsgFailed(("!IN_RING3"));
#endif  /* !IN_RING3 */

    if (!fPatchCode)
    {
        if (CTXSUFF(pVM->patm.s.pGCState)->GCPtrInhibitInterrupts == (RTRCPTR)pCtxCore->eip)
        {
            EMSetInhibitInterruptsPC(VMMGetCpu0(pVM), pCtxCore->eip);
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
VMMDECL(uint32_t) PATMRawGetEFlags(PVM pVM, PCCPUMCTXCORE pCtxCore)
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
VMMDECL(void) PATMRawSetEFlags(PVM pVM, PCPUMCTXCORE pCtxCore, uint32_t efl)
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
VMMDECL(bool) PATMShouldUseRawMode(PVM pVM, RTRCPTR pAddrGC)
{
    return (    PATMIsEnabled(pVM)
            && ((pAddrGC >= (RTRCPTR)pVM->patm.s.pPatchMemGC && pAddrGC < (RTRCPTR)((RTRCUINTPTR)pVM->patm.s.pPatchMemGC + pVM->patm.s.cbPatchMem)))) ? true : false;
}

/**
 * Returns the guest context pointer and size of the GC context structure
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMDECL(RCPTRTYPE(PPATMGCSTATE)) PATMQueryGCState(PVM pVM)
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
VMMDECL(bool) PATMIsPatchGCAddr(PVM pVM, RTRCPTR pAddrGC)
{
    return (PATMIsEnabled(pVM) && pAddrGC >= pVM->patm.s.pPatchMemGC && pAddrGC < (RTRCPTR)((RTRCUINTPTR)pVM->patm.s.pPatchMemGC + pVM->patm.s.cbPatchMem)) ? true : false;
}

/**
 * Set parameters for pending MMIO patch operation
 *
 * @returns VBox status code.
 * @param   pDevIns         Device instance.
 * @param   GCPhys          MMIO physical address
 * @param   pCachedData     GC pointer to cached data
 */
VMMDECL(int) PATMSetMMIOPatchInfo(PVM pVM, RTGCPHYS GCPhys, RTRCPTR pCachedData)
{
    pVM->patm.s.mmio.GCPhys = GCPhys;
    pVM->patm.s.mmio.pCachedData = (RTRCPTR)pCachedData;

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
VMMDECL(bool) PATMAreInterruptsEnabled(PVM pVM)
{
    PCPUMCTX pCtx = CPUMQueryGuestCtxPtr(VMMGetCpu0(pVM));

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
VMMDECL(bool) PATMAreInterruptsEnabledByCtxCore(PVM pVM, PCPUMCTXCORE pCtxCore)
{
    if (PATMIsEnabled(pVM))
    {
        if (PATMIsPatchGCAddr(pVM, (RTRCPTR)pCtxCore->eip))
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
VMMDECL(PPATMPATCHREC) PATMQueryFunctionPatch(PVM pVM, RTRCPTR pInstrGC)
{
    PPATMPATCHREC pRec;

    AssertCompile(sizeof(AVLOU32KEY) == sizeof(pInstrGC));
    pRec = (PPATMPATCHREC)RTAvloU32Get(&CTXSUFF(pVM->patm.s.PatchLookupTree)->PatchTree, (AVLOU32KEY)pInstrGC);
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
VMMDECL(bool) PATMIsInt3Patch(PVM pVM, RTRCPTR pInstrGC, uint32_t *pOpcode, uint32_t *pSize)
{
    PPATMPATCHREC pRec;

    pRec = (PPATMPATCHREC)RTAvloU32Get(&CTXSUFF(pVM->patm.s.PatchLookupTree)->PatchTree, (AVLOU32KEY)pInstrGC);
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
VMMDECL(int) PATMSysCall(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu)
{
    PCPUMCTX pCtx = CPUMQueryGuestCtxPtr(VMMGetCpu0(pVM));

    if (pCpu->pCurInstr->opcode == OP_SYSENTER)
    {
        if (    pCtx->SysEnter.cs == 0
            ||  pRegFrame->eflags.Bits.u1VM
            ||  (pRegFrame->cs & X86_SEL_RPL) != 3
            ||  pVM->patm.s.pfnSysEnterPatchGC == 0
            ||  pVM->patm.s.pfnSysEnterGC != (RTRCPTR)pCtx->SysEnter.eip
            ||  !(PATMRawGetEFlags(pVM, pRegFrame) & X86_EFL_IF))
            goto end;

        Log2(("PATMSysCall: sysenter from %RRv to %RRv\n", pRegFrame->eip, pVM->patm.s.pfnSysEnterPatchGC));
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
            ||  pRegFrame->eflags.Bits.u1VM
            ||  !(PATMRawGetEFlags(pVM, pRegFrame) & X86_EFL_IF))
            goto end;

        Log2(("PATMSysCall: sysexit from %RRv to %RRv\n", pRegFrame->eip, pRegFrame->edx));

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
 * Adds branch pair to the lookup cache of the particular branch instruction
 *
 * @returns VBox status
 * @param   pVM                 The VM to operate on.
 * @param   pJumpTableGC        Pointer to branch instruction lookup cache
 * @param   pBranchTarget       Original branch target
 * @param   pRelBranchPatch     Relative duplicated function address
 */
VMMDECL(int) PATMAddBranchToLookupCache(PVM pVM, RTRCPTR pJumpTableGC, RTRCPTR pBranchTarget, RTRCUINTPTR pRelBranchPatch)
{
    PPATCHJUMPTABLE pJumpTable;

    Log(("PATMAddBranchToLookupCache: Adding (%RRv->%RRv (%RRv)) to table %RRv\n", pBranchTarget, pRelBranchPatch + pVM->patm.s.pPatchMemGC, pRelBranchPatch, pJumpTableGC));

    AssertReturn(PATMIsPatchGCAddr(pVM, pJumpTableGC), VERR_INVALID_PARAMETER);

#ifdef IN_RC
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


#if defined(VBOX_WITH_STATISTICS) || defined(LOG_ENABLED)
/**
 * Return the name of the patched instruction
 *
 * @returns instruction name
 *
 * @param   opcode      DIS instruction opcode
 * @param   fPatchFlags Patch flags
 */
VMMDECL(const char *) patmGetInstructionString(uint32_t opcode, uint32_t fPatchFlags)
{
    const char *pszInstr = NULL;

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
#endif
