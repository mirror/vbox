/* $Id$ */
/** @file
 * PATM - Dynamic Guest OS Patching Manager - Raw-mode Context.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_PATM
#include <VBox/vmm/patm.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/stam.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/em.h>
#ifdef VBOX_WITH_IEM
# include <VBox/vmm/iem.h>
#endif
#include <VBox/vmm/selm.h>
#include <VBox/vmm/mm.h>
#include "PATMInternal.h"
#include "PATMA.h"
#include <VBox/vmm/vm.h>
#include <VBox/dbg.h>
#include <VBox/dis.h>
#include <VBox/disopcode.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/string.h>


/**
 * \#PF Virtual Handler callback for Guest access a page monitored by PATM
 *
 * @returns VBox status code (appropriate for trap handling and GC return).
 * @param   pVM         Pointer to the VM.
 * @param   uErrorCode   CPU Error code.
 * @param   pRegFrame   Trap register frame.
 * @param   pvFault     The fault address (cr2).
 * @param   pvRange     The base address of the handled virtual range.
 * @param   offRange    The offset of the access into this range.
 *                      (If it's a EIP range this is the EIP, if not it's pvFault.)
 */
VMMRCDECL(int) PATMGCMonitorPage(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPTR pvRange, uintptr_t offRange)
{
    NOREF(uErrorCode); NOREF(pRegFrame); NOREF(pvFault); NOREF(pvRange); NOREF(offRange);
    pVM->patm.s.pvFaultMonitor = (RTRCPTR)(RTRCUINTPTR)pvFault;
    return VINF_PATM_CHECK_PATCH_PAGE;
}


/**
 * Checks if the write is located on a page with was patched before.
 * (if so, then we are not allowed to turn on r/w)
 *
 * @returns VBox status
 * @param   pVM         Pointer to the VM.
 * @param   pRegFrame   CPU context
 * @param   GCPtr       GC pointer to write address
 * @param   cbWrite     Nr of bytes to write
 *
 */
VMMRCDECL(int) PATMGCHandleWriteToPatchPage(PVM pVM, PCPUMCTXCORE pRegFrame, RTRCPTR GCPtr, uint32_t cbWrite)
{
    RTGCUINTPTR          pWritePageStart, pWritePageEnd;
    PPATMPATCHPAGE       pPatchPage;

    /* Quick boundary check */
    if (    PAGE_ADDRESS(GCPtr) < PAGE_ADDRESS(pVM->patm.s.pPatchedInstrGCLowest)
        ||  PAGE_ADDRESS(GCPtr) > PAGE_ADDRESS(pVM->patm.s.pPatchedInstrGCHighest)
       )
       return VERR_PATCH_NOT_FOUND;

    STAM_PROFILE_ADV_START(&pVM->patm.s.StatPatchWriteDetect, a);

    pWritePageStart = (RTRCUINTPTR)GCPtr & PAGE_BASE_GC_MASK;
    pWritePageEnd   = ((RTRCUINTPTR)GCPtr + cbWrite - 1) & PAGE_BASE_GC_MASK;

    pPatchPage = (PPATMPATCHPAGE)RTAvloU32Get(CTXSUFF(&pVM->patm.s.PatchLookupTree)->PatchTreeByPage, (AVLOU32KEY)pWritePageStart);
    if (    !pPatchPage
        &&  pWritePageStart != pWritePageEnd
       )
    {
        pPatchPage = (PPATMPATCHPAGE)RTAvloU32Get(CTXSUFF(&pVM->patm.s.PatchLookupTree)->PatchTreeByPage, (AVLOU32KEY)pWritePageEnd);
    }

#ifdef LOG_ENABLED
    if (pPatchPage)
        Log(("PATMGCHandleWriteToPatchPage: Found page %RRv for write to %RRv %d bytes (page low:high %RRv:%RRv\n", pPatchPage->Core.Key, GCPtr, cbWrite, pPatchPage->pLowestAddrGC, pPatchPage->pHighestAddrGC));
#endif

    if (pPatchPage)
    {
        if (    pPatchPage->pLowestAddrGC  > (RTRCPTR)((RTRCUINTPTR)GCPtr + cbWrite - 1)
            ||  pPatchPage->pHighestAddrGC < (RTRCPTR)GCPtr)
        {
            /* This part of the page was not patched; try to emulate the instruction. */
            LogFlow(("PATMHandleWriteToPatchPage: Interpret %x accessing %RRv\n", pRegFrame->eip, GCPtr));
            int rc = EMInterpretInstruction(VMMGetCpu0(pVM), pRegFrame, (RTGCPTR)(RTRCUINTPTR)GCPtr);
            if (rc == VINF_SUCCESS)
            {
                STAM_COUNTER_INC(&pVM->patm.s.StatPatchWriteInterpreted);
                STAM_PROFILE_ADV_STOP(&pVM->patm.s.StatPatchWriteDetect, a);
                return VINF_SUCCESS;
            }
            STAM_COUNTER_INC(&pVM->patm.s.StatPatchWriteInterpretedFailed);
        }
        R3PTRTYPE(PPATCHINFO) *paPatch = (R3PTRTYPE(PPATCHINFO) *)MMHyperR3ToRC(pVM, pPatchPage->papPatch);

        /* Increase the invalid write counter for each patch that's registered for that page. */
        for (uint32_t i=0;i<pPatchPage->cCount;i++)
        {
            PPATCHINFO pPatch = (PPATCHINFO)MMHyperR3ToRC(pVM, paPatch[i]);

            pPatch->cInvalidWrites++;
        }

        STAM_PROFILE_ADV_STOP(&pVM->patm.s.StatPatchWriteDetect, a);
        return VINF_EM_RAW_EMULATE_INSTR;
    }

    STAM_PROFILE_ADV_STOP(&pVM->patm.s.StatPatchWriteDetect, a);
    return VERR_PATCH_NOT_FOUND;
}


/**
 * Checks if the illegal instruction was caused by a patched instruction
 *
 * @returns VBox status
 *
 * @param   pVM         Pointer to the VM.
 * @param   pCtxCore    The relevant core context.
 */
VMMDECL(int) PATMRCHandleIllegalInstrTrap(PVM pVM, PCPUMCTXCORE pRegFrame)
{
    PPATMPATCHREC pRec;
    PVMCPU pVCpu = VMMGetCpu0(pVM);
    int rc;

    /* Very important check -> otherwise we have a security leak. */
    AssertReturn(!pRegFrame->eflags.Bits.u1VM && (pRegFrame->ss.Sel & X86_SEL_RPL) == 1, VERR_ACCESS_DENIED);
    Assert(PATMIsPatchGCAddr(pVM, pRegFrame->eip));

    /* OP_ILLUD2 in PATM generated code? */
    if (CTXSUFF(pVM->patm.s.pGCState)->uPendingAction)
    {
        LogFlow(("PATMRC: Pending action %x at %x\n", CTXSUFF(pVM->patm.s.pGCState)->uPendingAction, pRegFrame->eip));

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
                AssertMsg(!pRegFrame->edi || PATMIsPatchGCAddr(pVM, pRegFrame->edi), ("edx = %x\n", pRegFrame->edi));

                Log(("PATMRC: lookup %x jump table=%x\n", pRegFrame->edx, pRegFrame->edi));

                pRec = PATMQueryFunctionPatch(pVM, (RTRCPTR)(pRegFrame->edx));
                if (pRec)
                {
                    if (pRec->patch.uState == PATCH_ENABLED)
                    {
                        RTGCUINTPTR pRelAddr = pRec->patch.pPatchBlockOffset;   /* make it relative */
                        rc = PATMAddBranchToLookupCache(pVM, (RTRCPTR)pRegFrame->edi, (RTRCPTR)pRegFrame->edx, pRelAddr);
                        if (rc == VINF_SUCCESS)
                        {
                            Log(("Patch block %RRv called as function\n", pRec->patch.pPrivInstrGC));
                            pRec->patch.flags |= PATMFL_CODE_REFERENCED;

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
                    /* Check first before trying to generate a function/trampoline patch. */
                    if (pVM->patm.s.fOutOfMemory)
                    {
                        pRegFrame->eip += PATM_ILLEGAL_INSTR_SIZE;
                        pRegFrame->eax = 0;     /* make it fault */
                        STAM_COUNTER_INC(&pVM->patm.s.StatFunctionNotFound);
                        return VINF_SUCCESS;
                    }
                    STAM_COUNTER_INC(&pVM->patm.s.StatFunctionNotFound);
                    return VINF_PATM_DUPLICATE_FUNCTION;
                }
            }

            case PATM_ACTION_DISPATCH_PENDING_IRQ:
                /* Parameters:
                 *  edi = GC address to jump to
                 */
                Log(("PATMRC: Dispatch pending interrupt; eip=%x->%x\n", pRegFrame->eip, pRegFrame->edi));

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
                Log(("PATMRC: Dispatch pending interrupt (iret); eip=%x->%x\n", pRegFrame->eip, pRegFrame->edi));
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
                Log(("PATMRC: Do iret to V86 code; eip=%x\n", pRegFrame->eip));
                Assert(pVM->patm.s.CTXSUFF(pGCState)->Restore.uFlags == (PATM_RESTORE_EAX|PATM_RESTORE_ECX));
                Assert(pVM->patm.s.CTXSUFF(pGCState)->fPIF == 0);

                pRegFrame->eax = pVM->patm.s.CTXSUFF(pGCState)->Restore.uEAX;
                pRegFrame->ecx = pVM->patm.s.CTXSUFF(pGCState)->Restore.uECX;
                pVM->patm.s.CTXSUFF(pGCState)->Restore.uFlags = 0;

                rc = EMInterpretIretV86ForPatm(pVM, pVCpu, pRegFrame);
                if (RT_SUCCESS(rc))
                {
                    STAM_COUNTER_INC(&pVM->patm.s.StatEmulIret);

                    /* We are no longer executing PATM code; set PIF again. */
                    pVM->patm.s.CTXSUFF(pGCState)->fPIF = 1;
                    PGMRZDynMapReleaseAutoSet(pVCpu);
                    CPUMGCCallV86Code(pRegFrame);
                    /* does not return */
                }
                else
                    STAM_COUNTER_INC(&pVM->patm.s.StatEmulIretFailed);
                return rc;
            }

#ifdef DEBUG
            case PATM_ACTION_LOG_CLI:
                Log(("PATMRC: CLI at %x (current IF=%d iopl=%d)\n", pRegFrame->eip, !!(pVM->patm.s.CTXSUFF(pGCState)->uVMFlags & X86_EFL_IF), X86_EFL_GET_IOPL(pVM->patm.s.CTXSUFF(pGCState)->uVMFlags) ));
                pRegFrame->eip += PATM_ILLEGAL_INSTR_SIZE;
                return VINF_SUCCESS;

            case PATM_ACTION_LOG_STI:
                Log(("PATMRC: STI at %x (current IF=%d iopl=%d)\n", pRegFrame->eip, !!(pVM->patm.s.CTXSUFF(pGCState)->uVMFlags & X86_EFL_IF), X86_EFL_GET_IOPL(pVM->patm.s.CTXSUFF(pGCState)->uVMFlags) ));
                pRegFrame->eip += PATM_ILLEGAL_INSTR_SIZE;
                return VINF_SUCCESS;

            case PATM_ACTION_LOG_POPF_IF1:
                Log(("PATMRC: POPF setting IF at %x (current IF=%d iopl=%d)\n", pRegFrame->eip, !!(pVM->patm.s.CTXSUFF(pGCState)->uVMFlags & X86_EFL_IF), X86_EFL_GET_IOPL(pVM->patm.s.CTXSUFF(pGCState)->uVMFlags)));
                pRegFrame->eip += PATM_ILLEGAL_INSTR_SIZE;
                return VINF_SUCCESS;

            case PATM_ACTION_LOG_POPF_IF0:
                Log(("PATMRC: POPF at %x (current IF=%d iopl=%d)\n", pRegFrame->eip, !!(pVM->patm.s.CTXSUFF(pGCState)->uVMFlags & X86_EFL_IF), X86_EFL_GET_IOPL(pVM->patm.s.CTXSUFF(pGCState)->uVMFlags)));
                pRegFrame->eip += PATM_ILLEGAL_INSTR_SIZE;
                return VINF_SUCCESS;

            case PATM_ACTION_LOG_PUSHF:
                Log(("PATMRC: PUSHF at %x (current IF=%d iopl=%d)\n", pRegFrame->eip, !!(pVM->patm.s.CTXSUFF(pGCState)->uVMFlags & X86_EFL_IF), X86_EFL_GET_IOPL(pVM->patm.s.CTXSUFF(pGCState)->uVMFlags) ));
                pRegFrame->eip += PATM_ILLEGAL_INSTR_SIZE;
                return VINF_SUCCESS;

            case PATM_ACTION_LOG_IF1:
                Log(("PATMRC: IF=1 escape from %x\n", pRegFrame->eip));
                pRegFrame->eip += PATM_ILLEGAL_INSTR_SIZE;
                return VINF_SUCCESS;

            case PATM_ACTION_LOG_IRET:
            {
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
                                Log(("PATMRC: IRET->VM stack frame: return address %04X:%x eflags=%08x ss:esp=%04X:%x\n", selCS, eip, uEFlags, selSS, esp));
                                Log(("PATMRC: IRET->VM stack frame: DS=%04X ES=%04X FS=%04X GS=%04X\n", selDS, selES, selFS, selGS));
                            }
                        }
                        else
                            Log(("PATMRC: IRET stack frame: return address %04X:%x eflags=%08x ss:esp=%04X:%x\n", selCS, eip, uEFlags, selSS, esp));
                    }
                    else
                        Log(("PATMRC: IRET stack frame: return address %04X:%x eflags=%08x\n", selCS, eip, uEFlags));
                }
                Log(("PATMRC: IRET from %x (IF->1) current eflags=%x\n", pRegFrame->eip, pVM->patm.s.CTXSUFF(pGCState)->uVMFlags));
                pRegFrame->eip += PATM_ILLEGAL_INSTR_SIZE;
                return VINF_SUCCESS;
            }

            case PATM_ACTION_LOG_GATE_ENTRY:
            {
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
                                Log(("PATMRC: GATE->VM stack frame: return address %04X:%x eflags=%08x ss:esp=%04X:%x\n", selCS, eip, uEFlags, selSS, esp));
                                Log(("PATMRC: GATE->VM stack frame: DS=%04X ES=%04X FS=%04X GS=%04X\n", selDS, selES, selFS, selGS));
                            }
                        }
                        else
                            Log(("PATMRC: GATE stack frame: return address %04X:%x eflags=%08x ss:esp=%04X:%x\n", selCS, eip, uEFlags, selSS, esp));
                    }
                    else
                        Log(("PATMRC: GATE stack frame: return address %04X:%x eflags=%08x\n", selCS, eip, uEFlags));
                }
                pRegFrame->eip += PATM_ILLEGAL_INSTR_SIZE;
                return VINF_SUCCESS;
            }

            case PATM_ACTION_LOG_RET:
                Log(("PATMRC: RET from %x to %x ESP=%x iopl=%d\n", pRegFrame->eip, pRegFrame->edx, pRegFrame->ebx, X86_EFL_GET_IOPL(pVM->patm.s.CTXSUFF(pGCState)->uVMFlags)));
                pRegFrame->eip += PATM_ILLEGAL_INSTR_SIZE;
                return VINF_SUCCESS;

            case PATM_ACTION_LOG_CALL:
                Log(("PATMRC: CALL to %RRv return addr %RRv ESP=%x iopl=%d\n", pVM->patm.s.CTXSUFF(pGCState)->GCCallPatchTargetAddr, pVM->patm.s.CTXSUFF(pGCState)->GCCallReturnAddr, pRegFrame->edx, X86_EFL_GET_IOPL(pVM->patm.s.CTXSUFF(pGCState)->uVMFlags)));
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
    AssertMsgFailed(("Unexpected OP_ILLUD2 in patch code at %x (pending action %x)!!!!\n", pRegFrame->eip, CTXSUFF(pVM->patm.s.pGCState)->uPendingAction));
    return VINF_EM_RAW_EMULATE_INSTR;
}

/**
 * Checks if the int 3 was caused by a patched instruction
 *
 * @returns Strict VBox status, includes all statuses that
 *          EMInterpretInstructionDisasState and
 * @retval  VINF_SUCCESS
 * @retval  VINF_PATM_PATCH_INT3
 * @retval  VINF_EM_RAW_EMULATE_INSTR
 *
 * @param   pVM         Pointer to the VM.
 * @param   pCtxCore    The relevant core context.
 */
VMMRCDECL(int) PATMRCHandleInt3PatchTrap(PVM pVM, PCPUMCTXCORE pRegFrame)
{
    PPATMPATCHREC pRec;
    int rc;

    AssertReturn(!pRegFrame->eflags.Bits.u1VM && (pRegFrame->ss.Sel & X86_SEL_RPL) == 1, VERR_ACCESS_DENIED);

    /* Int 3 in PATM generated code? (most common case) */
    if (PATMIsPatchGCAddr(pVM, pRegFrame->eip))
    {
        /* Note! Hardcoded assumption about it being a single byte int 3 instruction. */
        pRegFrame->eip--;
        return VINF_PATM_PATCH_INT3;
    }

    /** @todo could use simple caching here to speed things up. */
    pRec = (PPATMPATCHREC)RTAvloU32Get(&CTXSUFF(pVM->patm.s.PatchLookupTree)->PatchTree, (AVLOU32KEY)(pRegFrame->eip - 1));  /* eip is pointing to the instruction *after* 'int 3' already */
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
        if (pRec->patch.flags & PATMFL_INT3_REPLACEMENT)
        {
            /* eip is pointing to the instruction *after* 'int 3' already */
            pRegFrame->eip = pRegFrame->eip - 1;

            PATM_STAT_RUN_INC(&pRec->patch);

            Log(("PATMHandleInt3PatchTrap found int3 for %s at %x\n", patmGetInstructionString(pRec->patch.opcode, 0), pRegFrame->eip));

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

            DISCPUMODE enmCpuMode = SELMGetCpuModeFromSelector(VMMGetCpu0(pVM), pRegFrame->eflags, pRegFrame->cs.Sel, 0);
            if (enmCpuMode != DISCPUMODE_32BIT)
            {
                AssertFailed();
                return VINF_EM_RAW_EMULATE_INSTR;
            }

#ifdef VBOX_WITH_IEM
            VBOXSTRICTRC rcStrict;
            rcStrict = IEMExecOneWithPrefetchedByPC(VMMGetCpu0(pVM), pRegFrame, pRegFrame->rip,
                                                    pRec->patch.aPrivInstr, pRec->patch.cbPrivInstr);
            rc = VBOXSTRICTRC_TODO(rcStrict);
#else
            uint32_t    cbOp;
            DISCPUSTATE cpu;
            rc = DISInstr(&pRec->patch.aPrivInstr[0], enmCpuMode, &cpu, &cbOp);
            if (RT_FAILURE(rc))
            {
                Log(("DISCoreOne failed with %Rrc\n", rc));
                PATM_STAT_FAULT_INC(&pRec->patch);
                pRec->patch.cTraps++;
                return VINF_EM_RAW_EMULATE_INSTR;
            }

            rc = EMInterpretInstructionDisasState(VMMGetCpu0(pVM), &cpu, pRegFrame, 0 /* not relevant here */,
                                                  EMCODETYPE_SUPERVISOR);
#endif
            if (RT_FAILURE(rc))
            {
                Log(("EMInterpretInstructionCPU failed with %Rrc\n", rc));
                PATM_STAT_FAULT_INC(&pRec->patch);
                pRec->patch.cTraps++;
                return VINF_EM_RAW_EMULATE_INSTR;
            }
            return rc;
        }
    }
    return VERR_PATCH_NOT_FOUND;
}

