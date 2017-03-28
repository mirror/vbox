/* $Id$ */
/** @file
 * HM SVM (AMD-V) - All contexts.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_HM
#include "HMInternal.h"
#include <VBox/vmm/apic.h>
#include <VBox/vmm/gim.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/iem.h>
#include <VBox/vmm/vm.h>
#include <VBox/vmm/hm_svm.h>


#ifndef IN_RC
/**
 * Emulates a simple MOV TPR (CR8) instruction, used for TPR patching on 32-bit
 * guests. This simply looks up the patch record at EIP and does the required.
 *
 * This VMMCALL is used a fallback mechanism when mov to/from cr8 isn't exactly
 * like how we want it to be (e.g. not followed by shr 4 as is usually done for
 * TPR). See hmR3ReplaceTprInstr() for the details.
 *
 * @returns VBox status code.
 * @retval VINF_SUCCESS if the access was handled successfully.
 * @retval VERR_NOT_FOUND if no patch record for this RIP could be found.
 * @retval VERR_SVM_UNEXPECTED_PATCH_TYPE if the found patch type is invalid.
 *
 * @param   pVCpu               The cross context virtual CPU structure.
 * @param   pCtx                Pointer to the guest-CPU context.
 * @param   pfUpdateRipAndRF    Whether the guest RIP/EIP has been updated as
 *                              part of the TPR patch operation.
 */
static int hmSvmEmulateMovTpr(PVMCPU pVCpu, PCPUMCTX pCtx, bool *pfUpdateRipAndRF)
{
    Log4(("Emulated VMMCall TPR access replacement at RIP=%RGv\n", pCtx->rip));

    /*
     * We do this in a loop as we increment the RIP after a successful emulation
     * and the new RIP may be a patched instruction which needs emulation as well.
     */
    bool fUpdateRipAndRF = false;
    bool fPatchFound     = false;
    PVM  pVM = pVCpu->CTX_SUFF(pVM);
    for (;;)
    {
        bool    fPending;
        uint8_t u8Tpr;

        PHMTPRPATCH pPatch = (PHMTPRPATCH)RTAvloU32Get(&pVM->hm.s.PatchTree, (AVLOU32KEY)pCtx->eip);
        if (!pPatch)
            break;

        fPatchFound = true;
        switch (pPatch->enmType)
        {
            case HMTPRINSTR_READ:
            {
                int rc = APICGetTpr(pVCpu, &u8Tpr, &fPending, NULL /* pu8PendingIrq */);
                AssertRC(rc);

                rc = DISWriteReg32(CPUMCTX2CORE(pCtx), pPatch->uDstOperand, u8Tpr);
                AssertRC(rc);
                pCtx->rip += pPatch->cbOp;
                pCtx->eflags.Bits.u1RF = 0;
                fUpdateRipAndRF = true;
                break;
            }

            case HMTPRINSTR_WRITE_REG:
            case HMTPRINSTR_WRITE_IMM:
            {
                if (pPatch->enmType == HMTPRINSTR_WRITE_REG)
                {
                    uint32_t u32Val;
                    int rc = DISFetchReg32(CPUMCTX2CORE(pCtx), pPatch->uSrcOperand, &u32Val);
                    AssertRC(rc);
                    u8Tpr = u32Val;
                }
                else
                    u8Tpr = (uint8_t)pPatch->uSrcOperand;

                int rc2 = APICSetTpr(pVCpu, u8Tpr);
                AssertRC(rc2);
                HMCPU_CF_SET(pVCpu, HM_CHANGED_SVM_GUEST_APIC_STATE);

                pCtx->rip += pPatch->cbOp;
                pCtx->eflags.Bits.u1RF = 0;
                fUpdateRipAndRF = true;
                break;
            }

            default:
            {
                AssertMsgFailed(("Unexpected patch type %d\n", pPatch->enmType));
                pVCpu->hm.s.u32HMError = pPatch->enmType;
                *pfUpdateRipAndRF = fUpdateRipAndRF;
                return VERR_SVM_UNEXPECTED_PATCH_TYPE;
            }
        }
    }

    *pfUpdateRipAndRF = fUpdateRipAndRF;
    if (fPatchFound)
        return VINF_SUCCESS;
    return VERR_NOT_FOUND;
}
#endif /* !IN_RC */


/**
 * Performs the operations necessary that are part of the vmmcall instruction
 * execution in the guest.
 *
 * @returns Strict VBox status code (i.e. informational status codes too).
 * @retval  VINF_SUCCESS on successful handling, no \#UD needs to be thrown,
 *          update RIP and eflags.RF depending on @a pfUpdatedRipAndRF and
 *          continue guest execution.
 * @retval  VINF_GIM_HYPERCALL_CONTINUING continue hypercall without updating
 *          RIP.
 * @retval  VINF_GIM_R3_HYPERCALL re-start the hypercall from ring-3.
 *
 * @param   pVCpu               The cross context virtual CPU structure.
 * @param   pCtx                Pointer to the guest-CPU context.
 * @param   pfUpdatedRipAndRF   Whether the guest RIP/EIP has been updated as
 *                              part of handling the VMMCALL operation.
 */
VMM_INT_DECL(VBOXSTRICTRC) HMSvmVmmcall(PVMCPU pVCpu, PCPUMCTX pCtx, bool *pfUpdatedRipAndRF)
{
#ifndef IN_RC
    /*
     * TPR patched instruction emulation for 32-bit guests.
     */
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    if (pVM->hm.s.fTprPatchingAllowed)
    {
        int rc = hmSvmEmulateMovTpr(pVCpu, pCtx, pfUpdatedRipAndRF);
        if (RT_SUCCESS(rc))
            return VINF_SUCCESS;

        if (rc != VERR_NOT_FOUND)
        {
            Log(("hmSvmExitVmmCall: hmSvmEmulateMovTpr returns %Rrc\n", rc));
            return rc;
        }
    }
#endif

    /*
     * Paravirtualized hypercalls.
     */
    *pfUpdatedRipAndRF = false;
    if (pVCpu->hm.s.fHypercallsEnabled)
        return GIMHypercall(pVCpu, pCtx);

    return VERR_NOT_AVAILABLE;
}


/**
 * Performs the operations necessary that are part of the vmrun instruction
 * execution in the guest.
 *
 * @returns Strict VBox status code (i.e. informational status codes too).
 * @retval  VINF_SUCCESS successully executed VMRUN and entered nested-guest
 *          code execution.
 * @retval  VINF_SVM_VMEXIT when executing VMRUN causes a \#VMEXIT
 *          (SVM_EXIT_INVALID most likely).
 *
 * @param   pVCpu               The cross context virtual CPU structure.
 * @param   pCtx                Pointer to the guest-CPU context.
 * @param   GCPhysVmcb          Guest physical address of the VMCB to run.
 */
/** @todo move this to IEM and make the VMRUN version that can execute under
 *        hardware SVM here instead. */
VMM_INT_DECL(VBOXSTRICTRC) HMSvmVmrun(PVMCPU pVCpu, PCPUMCTX pCtx, RTGCPHYS GCPhysVmcb)
{
    Assert(pVCpu);
    Assert(pCtx);
    PVM pVM = pVCpu->CTX_SUFF(pVM);

    /*
     * Cache the physical address of the VMCB for #VMEXIT exceptions.
     */
    pCtx->hwvirt.svm.GCPhysVmcb = GCPhysVmcb;

    /*
     * Save host state.
     */
    SVMVMCBSTATESAVE VmcbNstGst;
    int rc = PGMPhysSimpleReadGCPhys(pVM, &VmcbNstGst, GCPhysVmcb + RT_OFFSETOF(SVMVMCB, guest), sizeof(SVMVMCBSTATESAVE));
    if (RT_SUCCESS(rc))
    {
        PSVMHOSTSTATE pHostState = &pCtx->hwvirt.svm.HostState;
        pHostState->es       = pCtx->es;
        pHostState->cs       = pCtx->cs;
        pHostState->ss       = pCtx->ss;
        pHostState->ds       = pCtx->ds;
        pHostState->gdtr     = pCtx->gdtr;
        pHostState->idtr     = pCtx->idtr;
        pHostState->uEferMsr = pCtx->msrEFER;
        pHostState->uCr0     = pCtx->cr0;
        pHostState->uCr3     = pCtx->cr3;
        pHostState->uCr4     = pCtx->cr4;
        pHostState->rflags   = pCtx->rflags;
        pHostState->uRip     = pCtx->rip;
        pHostState->uRsp     = pCtx->rsp;
        pHostState->uRax     = pCtx->rax;

        /*
         * Load the VMCB controls.
         */
        rc = PGMPhysSimpleReadGCPhys(pVM, &pCtx->hwvirt.svm.VmcbCtrl, GCPhysVmcb, sizeof(pCtx->hwvirt.svm.VmcbCtrl));
        if (RT_SUCCESS(rc))
        {
            PSVMVMCBCTRL pVmcbCtrl = &pCtx->hwvirt.svm.VmcbCtrl;

            /*
             * Validate guest-state and controls.
             */
            /* VMRUN must always be intercepted. */
            if (!CPUMIsGuestSvmCtrlInterceptSet(pCtx, SVM_CTRL_INTERCEPT_VMRUN))
            {
                Log(("HMSvmVmRun: VMRUN instruction not intercepted -> #VMEXIT\n"));
                return HMSvmNstGstVmExit(pVCpu, pCtx, SVM_EXIT_INVALID, 0 /* uExitInfo1 */, 0 /* uExitInfo2 */);
            }

            /* Nested paging. */
            if (    pVmcbCtrl->NestedPaging.n.u1NestedPaging
                && !pVM->cpum.ro.GuestFeatures.svm.feat.n.fNestedPaging)
            {
                Log(("HMSvmVmRun: Nested paging not supported -> #VMEXIT\n"));
                return HMSvmNstGstVmExit(pVCpu, pCtx, SVM_EXIT_INVALID, 0 /* uExitInfo1 */, 0 /* uExitInfo2 */);
            }

            /* AVIC. */
            if (    pVmcbCtrl->IntCtrl.n.u1AvicEnable
                && !pVM->cpum.ro.GuestFeatures.svm.feat.n.fAvic)
            {
                Log(("HMSvmVmRun: AVIC not supported -> #VMEXIT\n"));
                return HMSvmNstGstVmExit(pVCpu, pCtx, SVM_EXIT_INVALID, 0 /* uExitInfo1 */, 0 /* uExitInfo2 */);
            }

            /* Last branch record (LBR) virtualization. */
            if (    (pVmcbCtrl->u64LBRVirt & SVM_LBR_VIRT_ENABLE)
                && !pVM->cpum.ro.GuestFeatures.svm.feat.n.fLbrVirt)
            {
                Log(("HMSvmVmRun: LBR virtualization not supported -> #VMEXIT\n"));
                return HMSvmNstGstVmExit(pVCpu, pCtx, SVM_EXIT_INVALID, 0 /* uExitInfo1 */, 0 /* uExitInfo2 */);
            }

            /* Guest ASID. */
            if (!pVmcbCtrl->TLBCtrl.n.u32ASID)
            {
                Log(("HMSvmVmRun: Guest ASID is invalid -> #VMEXIT\n"));
                return HMSvmNstGstVmExit(pVCpu, pCtx, SVM_EXIT_INVALID, 0 /* uExitInfo1 */, 0 /* uExitInfo2 */);
            }

            /* IO permission bitmap. */
            RTGCPHYS GCPhysIOBitmap = pVmcbCtrl->u64IOPMPhysAddr;
            if (   (GCPhysIOBitmap & X86_PAGE_4K_OFFSET_MASK)
                || !PGMPhysIsGCPhysNormal(pVM, GCPhysIOBitmap))
            {
                Log(("HMSvmVmRun: IO bitmap physaddr invalid. GCPhysIOBitmap=%#RX64 -> #VMEXIT\n", GCPhysIOBitmap));
                return HMSvmNstGstVmExit(pVCpu, pCtx, SVM_EXIT_INVALID, 0 /* uExitInfo1 */, 0 /* uExitInfo2 */);
            }

            /* MSR permission bitmap. */
            RTGCPHYS GCPhysMsrBitmap = pVmcbCtrl->u64MSRPMPhysAddr;
            if (   (GCPhysMsrBitmap & X86_PAGE_4K_OFFSET_MASK)
                || !PGMPhysIsGCPhysNormal(pVM, GCPhysMsrBitmap))
            {
                Log(("HMSvmVmRun: MSR bitmap physaddr invalid. GCPhysMsrBitmap=%#RX64 -> #VMEXIT\n", GCPhysMsrBitmap));
                return HMSvmNstGstVmExit(pVCpu, pCtx, SVM_EXIT_INVALID, 0 /* uExitInfo1 */, 0 /* uExitInfo2 */);
            }

            /* CR0. */
            if (   !(VmcbNstGst.u64CR0 & X86_CR0_CD)
                &&  (VmcbNstGst.u64CR0 & X86_CR0_NW))
            {
                Log(("HMSvmVmRun: CR0 no-write through with cache disabled. CR0=%#RX64 -> #VMEXIT\n", VmcbNstGst.u64CR0));
                return HMSvmNstGstVmExit(pVCpu, pCtx, SVM_EXIT_INVALID, 0 /* uExitInfo1 */, 0 /* uExitInfo2 */);
            }
            if (VmcbNstGst.u64CR0 >> 32)
            {
                Log(("HMSvmVmRun: CR0 reserved bits set. CR0=%#RX64 -> #VMEXIT\n", VmcbNstGst.u64CR0));
                return HMSvmNstGstVmExit(pVCpu, pCtx, SVM_EXIT_INVALID, 0 /* uExitInfo1 */, 0 /* uExitInfo2 */);
            }
            /** @todo Implement all reserved bits/illegal combinations for CR3, CR4. */

            /* DR6 and DR7. */
            if (   VmcbNstGst.u64DR6 >> 32
                || VmcbNstGst.u64DR7 >> 32)
            {
                Log(("HMSvmVmRun: DR6 and/or DR7 reserved bits set. DR6=%#RX64 DR7=%#RX64 -> #VMEXIT\n", VmcbNstGst.u64DR6,
                     VmcbNstGst.u64DR6));
                return HMSvmNstGstVmExit(pVCpu, pCtx, SVM_EXIT_INVALID, 0 /* uExitInfo1 */, 0 /* uExitInfo2 */);
            }

            /** @todo gPAT MSR validation? */

            /*
             * Copy segments from nested-guest VMCB state to the guest-CPU state.
             *
             * We do this here as we need to use the CS attributes and it's easier this way
             * then using the VMCB format selectors. It doesn't really matter where we copy
             * the state, we restore the guest-CPU context state on the \#VMEXIT anyway.
             */
            HMSVM_SEG_REG_COPY_FROM_VMCB(pCtx, &VmcbNstGst, ES, es);
            HMSVM_SEG_REG_COPY_FROM_VMCB(pCtx, &VmcbNstGst, CS, cs);
            HMSVM_SEG_REG_COPY_FROM_VMCB(pCtx, &VmcbNstGst, SS, ss);
            HMSVM_SEG_REG_COPY_FROM_VMCB(pCtx, &VmcbNstGst, DS, ds);

            /** @todo Segment attribute overrides by VMRUN. */

            /*
             * CPL adjustments and overrides.
             *
             * SS.DPL is apparently the CPU's CPL, see comment in CPUMGetGuestCPL().
             * We shall thus adjust both CS.DPL and SS.DPL here.
             */
            pCtx->cs.Attr.n.u2Dpl = pCtx->ss.Attr.n.u2Dpl = VmcbNstGst.u8CPL;
            if (CPUMIsGuestInV86ModeEx(pCtx))
                pCtx->cs.Attr.n.u2Dpl = pCtx->ss.Attr.n.u2Dpl = 3;
            if (CPUMIsGuestInRealModeEx(pCtx))
                pCtx->cs.Attr.n.u2Dpl = pCtx->ss.Attr.n.u2Dpl = 0;

            /*
             * Continue validating guest-state and controls.
             */
            /* EFER, CR0 and CR4. */
            uint64_t uValidEfer;
            rc = CPUMGetValidateEfer(pVM, VmcbNstGst.u64CR0, 0 /* uOldEfer */, VmcbNstGst.u64EFER, &uValidEfer);
            if (RT_FAILURE(rc))
            {
                Log(("HMSvmVmRun: EFER invalid uOldEfer=%#RX64 uValidEfer=%#RX64 -> #VMEXIT\n", VmcbNstGst.u64EFER, uValidEfer));
                return HMSvmNstGstVmExit(pVCpu, pCtx, SVM_EXIT_INVALID, 0 /* uExitInfo1 */, 0 /* uExitInfo2 */);
            }
            bool const fSvm                     = RT_BOOL(uValidEfer & MSR_K6_EFER_SVME);
            bool const fLongModeSupported       = RT_BOOL(pVM->cpum.ro.GuestFeatures.fLongMode);
            bool const fLongModeEnabled         = RT_BOOL(uValidEfer & MSR_K6_EFER_LME);
            bool const fPaging                  = RT_BOOL(VmcbNstGst.u64CR0 & X86_CR0_PG);
            bool const fPae                     = RT_BOOL(VmcbNstGst.u64CR4 & X86_CR4_PAE);
            bool const fProtMode                = RT_BOOL(VmcbNstGst.u64CR0 & X86_CR0_PE);
            bool const fLongModeWithPaging      = fLongModeEnabled && fPaging;
            bool const fLongModeConformCS       = pCtx->cs.Attr.n.u1Long && pCtx->cs.Attr.n.u1DefBig;
            /* Adjust EFER.LMA (this is normally done by the CPU when system software writes CR0). */
            if (fLongModeWithPaging)
                uValidEfer |= MSR_K6_EFER_LMA;
            bool const fLongModeActiveOrEnabled = RT_BOOL(uValidEfer & (MSR_K6_EFER_LME | MSR_K6_EFER_LMA));
            if (   !fSvm
                || (!fLongModeSupported && fLongModeActiveOrEnabled)
                || (fLongModeWithPaging && !fPae)
                || (fLongModeWithPaging && !fProtMode)
                || (   fLongModeEnabled
                    && fPaging
                    && fPae
                    && fLongModeConformCS))
            {
                Log(("HMSvmVmRun: EFER invalid. uValidEfer=%#RX64 -> #VMEXIT\n", uValidEfer));
                return HMSvmNstGstVmExit(pVCpu, pCtx, SVM_EXIT_INVALID, 0 /* uExitInfo1 */, 0 /* uExitInfo2 */);
            }

            /*
             * Preserve the required force-flags.
             *
             * We only preserve the force-flags that would affect the execution of the
             * nested-guest (or the guest).
             *
             *   - VMCPU_FF_INHIBIT_INTERRUPTS need not be preserved as it's for a single
             *     instruction which is this VMRUN instruction itself.
             *
             *   - VMCPU_FF_BLOCK_NMIS needs to be preserved as it blocks NMI until the
             *     execution of a subsequent IRET instruction in the guest.
             *
             *   - The remaining FFs (e.g. timers) can stay in place so that we will be
             *     able to generate interrupts that should cause #VMEXITs for the
             *     nested-guest.
             */
            /** @todo anything missed more here? */
            pCtx->hwvirt.fLocalForcedActions = pVCpu->fLocalForcedActions & VMCPU_FF_BLOCK_NMIS;

            /*
             * Interrupt shadow.
             */
            if (pVmcbCtrl->u64IntShadow & SVM_INTERRUPT_SHADOW_ACTIVE)
                EMSetInhibitInterruptsPC(pVCpu, VmcbNstGst.u64RIP);

            /*
             * TLB flush control.
             */
            /** @todo @bugref{7243}: ASID based PGM TLB flushes. */
            if (   pVmcbCtrl->TLBCtrl.n.u8TLBFlush == SVM_TLB_FLUSH_ENTIRE
                || pVmcbCtrl->TLBCtrl.n.u8TLBFlush == SVM_TLB_FLUSH_SINGLE_CONTEXT
                || pVmcbCtrl->TLBCtrl.n.u8TLBFlush == SVM_TLB_FLUSH_SINGLE_CONTEXT_RETAIN_GLOBALS)
                PGMFlushTLB(pVCpu, VmcbNstGst.u64CR3, true /* fGlobal */);

            /** @todo @bugref{7243}: SVM TSC offset, see tmCpuTickGetInternal. */

            /*
             * Copy the remaining guest state from the VMCB to the guest-CPU context.
             */
            pCtx->gdtr.cbGdt = VmcbNstGst.GDTR.u32Limit;
            pCtx->gdtr.pGdt  = VmcbNstGst.GDTR.u64Base;
            pCtx->idtr.cbIdt = VmcbNstGst.IDTR.u32Limit;
            pCtx->idtr.pIdt  = VmcbNstGst.IDTR.u64Base;
            pCtx->cr0        = VmcbNstGst.u64CR0;   /** @todo What about informing PGM about CR0.WP? */
            pCtx->cr4        = VmcbNstGst.u64CR4;
            pCtx->cr3        = VmcbNstGst.u64CR3;
            pCtx->cr2        = VmcbNstGst.u64CR2;
            pCtx->dr[6]      = VmcbNstGst.u64DR6;
            pCtx->dr[7]      = VmcbNstGst.u64DR7;
            pCtx->rflags.u   = VmcbNstGst.u64RFlags;
            pCtx->rax        = VmcbNstGst.u64RAX;
            pCtx->rsp        = VmcbNstGst.u64RSP;
            pCtx->rip        = VmcbNstGst.u64RIP;
            pCtx->msrEFER    = uValidEfer;

            /* Mask DR6, DR7 bits mandatory set/clear bits. */
            pCtx->dr[6] &= ~(X86_DR6_RAZ_MASK | X86_DR6_MBZ_MASK);
            pCtx->dr[6] |= X86_DR6_RA1_MASK;
            pCtx->dr[7] &= ~(X86_DR7_RAZ_MASK | X86_DR7_MBZ_MASK);
            pCtx->dr[7] |= X86_DR7_RA1_MASK;

            /*
             * Check for pending virtual interrupts.
             */
            if (pVmcbCtrl->IntCtrl.n.u1VIrqPending)
                VMCPU_FF_SET(pVCpu, VMCPU_FF_INTERRUPT_NESTED_GUEST);

            /*
             * Clear global interrupt flags to allow interrupts in the guest.
             */
            pCtx->hwvirt.svm.fGif = 1;

            /*
             * Event injection.
             */
            PCSVMEVENT pEventInject = &pVmcbCtrl->EventInject;
            if (pEventInject->n.u1Valid)
            {
                uint8_t   const uVector    = pEventInject->n.u8Vector;
                TRPMEVENT const enmType    = hmSvmEventToTrpmEventType(pEventInject);
                uint16_t  const uErrorCode = pEventInject->n.u1ErrorCodeValid ? pEventInject->n.u32ErrorCode : 0;

                /* Validate vectors for hardware exceptions, see AMD spec. 15.20 "Event Injection". */
                if (enmType == TRPM_32BIT_HACK)
                {
                    Log(("HMSvmVmRun: Invalid event type =%#x -> #VMEXIT\n", (uint8_t)pEventInject->n.u3Type));
                    return HMSvmNstGstVmExit(pVCpu, pCtx, SVM_EXIT_INVALID, 0 /* uExitInfo1 */, 0 /* uExitInfo2 */);
                }
                if (pEventInject->n.u3Type == SVM_EVENT_EXCEPTION)
                {
                    if (   uVector == X86_XCPT_NMI
                        || uVector > 31 /* X86_XCPT_MAX */)
                    {
                        Log(("HMSvmVmRun: Invalid vector for hardware exception. uVector=%#x -> #VMEXIT\n", uVector));
                        return HMSvmNstGstVmExit(pVCpu, pCtx, SVM_EXIT_INVALID, 0 /* uExitInfo1 */, 0 /* uExitInfo2 */);
                    }
                    if (   uVector == X86_XCPT_BR
                        && CPUMIsGuestInLongModeEx(pCtx))
                    {
                        Log(("HMSvmVmRun: Cannot inject #BR when not in long mode -> #VMEXIT\n"));
                        return HMSvmNstGstVmExit(pVCpu, pCtx, SVM_EXIT_INVALID, 0 /* uExitInfo1 */, 0 /* uExitInfo2 */);
                    }
                    /** @todo any others? */
                }

                /*
                 * Update the exit interruption info field so that if an exception occurs
                 * while delivering the event causing a #VMEXIT, we only need to update
                 * the valid bit while the rest is already in place.
                 */
                pVmcbCtrl->ExitIntInfo.u = pVmcbCtrl->EventInject.u;
                pVmcbCtrl->ExitIntInfo.n.u1Valid = 0;

                /** @todo NRIP: Software interrupts can only be pushed properly if we support
                 *        NRIP for the nested-guest to calculate the instruction length
                 *        below. */
                VBOXSTRICTRC rcStrict = IEMInjectTrap(pVCpu, uVector, enmType, uErrorCode, pCtx->cr2, 0 /* cbInstr */);
                if (rcStrict == VINF_SVM_VMEXIT)
                    return rcStrict;
            }

            return VINF_SUCCESS;
        }

        /* Shouldn't really happen as the caller should've validated the physical address already. */
        Log(("HMSvmVmRun: Failed to read nested-guest VMCB control area at %#RGp -> #VMEXIT\n",
             GCPhysVmcb));
        return VERR_SVM_IPE_4;
    }

    /* Shouldn't really happen as the caller should've validated the physical address already. */
    Log(("HMSvmVmRun: Failed to read nested-guest VMCB save-state area at %#RGp -> #VMEXIT\n",
         GCPhysVmcb + RT_OFFSETOF(SVMVMCB, guest)));
    return VERR_SVM_IPE_5;
}


/**
 * SVM nested-guest \#VMEXIT handler.
 *
 * @returns Strict VBox status code.
 * @retval VINF_SVM_VMEXIT when the \#VMEXIT is successful.
 * @retval VERR_SVM_VMEXIT_FAILED when the \#VMEXIT failed restoring the guest's
 *         "host state" and a shutdown is required.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pCtx        The guest-CPU context.
 * @param   uExitCode   The exit code.
 * @param   uExitInfo1  The exit info. 1 field.
 * @param   uExitInfo2  The exit info. 2 field.
 */
VMM_INT_DECL(VBOXSTRICTRC) HMSvmNstGstVmExit(PVMCPU pVCpu, PCPUMCTX pCtx, uint64_t uExitCode, uint64_t uExitInfo1,
                                             uint64_t uExitInfo2)
{
    if (   CPUMIsGuestInNestedHwVirtMode(pCtx)
        || uExitCode == SVM_EXIT_INVALID)
    {
        RT_NOREF(pVCpu);

        pCtx->hwvirt.svm.fGif = 0;
#ifdef VBOX_STRICT
        RT_ZERO(pCtx->hwvirt.svm.VmcbCtrl);
        RT_ZERO(pCtx->hwvirt.svm.HostState);
        pCtx->hwvirt.svm.GCPhysVmcb = NIL_RTGCPHYS;
#endif

        /*
         * Save the nested-guest state into the VMCB state-save area.
         */
        SVMVMCBSTATESAVE VmcbNstGst;
        HMSVM_SEG_REG_COPY_TO_VMCB(pCtx, &VmcbNstGst, ES, es);
        HMSVM_SEG_REG_COPY_TO_VMCB(pCtx, &VmcbNstGst, CS, cs);
        HMSVM_SEG_REG_COPY_TO_VMCB(pCtx, &VmcbNstGst, SS, ss);
        HMSVM_SEG_REG_COPY_TO_VMCB(pCtx, &VmcbNstGst, DS, ds);
        VmcbNstGst.GDTR.u32Limit = pCtx->gdtr.cbGdt;
        VmcbNstGst.GDTR.u64Base  = pCtx->gdtr.pGdt;
        VmcbNstGst.IDTR.u32Limit = pCtx->idtr.cbIdt;
        VmcbNstGst.IDTR.u32Limit = pCtx->idtr.pIdt;
        VmcbNstGst.u64EFER       = pCtx->msrEFER;
        VmcbNstGst.u64CR4        = pCtx->cr4;
        VmcbNstGst.u64CR3        = pCtx->cr3;
        VmcbNstGst.u64CR2        = pCtx->cr2;
        VmcbNstGst.u64CR0        = pCtx->cr0;
        /** @todo Nested paging. */
        VmcbNstGst.u64RFlags     = pCtx->rflags.u64;
        VmcbNstGst.u64RIP        = pCtx->rip;
        VmcbNstGst.u64RSP        = pCtx->rsp;
        VmcbNstGst.u64RAX        = pCtx->rax;
        VmcbNstGst.u64DR7        = pCtx->dr[6];
        VmcbNstGst.u64DR6        = pCtx->dr[7];
        VmcbNstGst.u8CPL         = pCtx->ss.Attr.n.u2Dpl;   /* See comment in CPUMGetGuestCPL(). */

        /* Save interrupt shadow of the nested-guest instruction if any. */
        if (   VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS)
            && EMGetInhibitInterruptsPC(pVCpu) == pCtx->rip)
        {
            RT_ZERO(pCtx->hwvirt.svm.VmcbCtrl);
            pCtx->hwvirt.svm.VmcbCtrl.u64IntShadow |= SVM_INTERRUPT_SHADOW_ACTIVE;
        }

        /*
         * Save additional state and intercept information.
         */
        if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INTERRUPT_NESTED_GUEST))
        {
            Assert(pCtx->hwvirt.svm.VmcbCtrl.IntCtrl.n.u1VIrqPending);
            Assert(pCtx->hwvirt.svm.VmcbCtrl.IntCtrl.n.u8VIntrVector);
        }
        /** @todo Save V_TPR, V_IRQ. */
        /** @todo NRIP. */

        /* Save exit information. */
        pCtx->hwvirt.svm.VmcbCtrl.u64ExitCode  = uExitCode;
        pCtx->hwvirt.svm.VmcbCtrl.u64ExitInfo1 = uExitInfo1;
        pCtx->hwvirt.svm.VmcbCtrl.u64ExitInfo2 = uExitInfo2;

        /*
         * Clear event injection in the VMCB.
         */
        pCtx->hwvirt.svm.VmcbCtrl.EventInject.n.u1Valid = 0;

        /*
         * Write back the VMCB controls to the guest VMCB in guest physical memory.
         */
        int rc = PGMPhysSimpleWriteGCPhys(pVCpu->CTX_SUFF(pVM), pCtx->hwvirt.svm.GCPhysVmcb, &pCtx->hwvirt.svm.VmcbCtrl,
                                      sizeof(pCtx->hwvirt.svm.VmcbCtrl));
        if (RT_SUCCESS(rc))
        {
            /*
             * Prepare for guest's "host mode" by clearing internal processor state bits.
             *
             * Some of these like TSC offset can then be used unconditionally in our TM code
             * but the offset in the guest's VMCB will remain as it should as we've written
             * back the VMCB controls above.
             */
            RT_ZERO(pCtx->hwvirt.svm.VmcbCtrl);
#if 0
            /* Clear TSC offset. */
            pCtx->hwvirt.svm.VmcbCtrl.u64TSCOffset = 0;
            pCtx->hwvirt.svm.VmcbCtrl.IntCtrl.n.u1VIrqValid = 0;
            pCtx->hwvirt.svm.VmcbCtrl.IntCtrl.n.u1VIntrMasking = 0;
#endif
            /* Restore guest's force-flags. */
            if (pCtx->hwvirt.fLocalForcedActions)
                VMCPU_FF_SET(pVCpu, pCtx->hwvirt.fLocalForcedActions);

            /* Clear nested-guest's interrupt pending. */
            if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INTERRUPT_NESTED_GUEST))
                VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_NESTED_GUEST);

            /** @todo Nested paging. */
            /** @todo ASID. */

            /*
             * Reload the guest's "host state".
             */
            PSVMHOSTSTATE pHostState = &pCtx->hwvirt.svm.HostState;
            pCtx->es         = pHostState->es;
            pCtx->cs         = pHostState->cs;
            pCtx->ss         = pHostState->ss;
            pCtx->ds         = pHostState->ds;
            pCtx->gdtr       = pHostState->gdtr;
            pCtx->idtr       = pHostState->idtr;
            pCtx->msrEFER    = pHostState->uEferMsr;
            pCtx->cr0        = pHostState->uCr0 | X86_CR0_PE;
            pCtx->cr3        = pHostState->uCr3;
            pCtx->cr4        = pHostState->uCr4;
            pCtx->rflags     = pHostState->rflags;
            pCtx->rflags.Bits.u1VM = 0;
            pCtx->rip        = pHostState->uRip;
            pCtx->rsp        = pHostState->uRsp;
            pCtx->rax        = pHostState->uRax;
            /* The spec says "Disables all hardware breakpoints in DR7"... */
            pCtx->dr[7]     &= ~(X86_DR7_ENABLED_MASK | X86_DR7_RAZ_MASK | X86_DR7_MBZ_MASK);
            pCtx->dr[7]     |= X86_DR7_RA1_MASK;

            rc = VINF_SVM_VMEXIT;
        }
        else
        {
            Log(("HMNstGstSvmVmExit: Writing VMCB at %#RGp failed\n", pCtx->hwvirt.svm.GCPhysVmcb));
            rc = VERR_SVM_VMEXIT_FAILED;
        }

        return rc;
    }

    Log(("HMNstGstSvmVmExit: Not in SVM guest mode! uExitCode=%#RX64 uExitInfo1=%#RX64 uExitInfo2=%#RX64\n", uExitCode,
         uExitInfo1, uExitInfo2));
    RT_NOREF2(uExitInfo1, uExitInfo2);
    return VERR_SVM_IPE_5;
}


/**
 * Converts an SVM event type to a TRPM event type.
 *
 * @returns The TRPM event type.
 * @retval  TRPM_32BIT_HACK if the specified type of event isn't among the set
 *          of recognized trap types.
 *
 * @param   pEvent       Pointer to the SVM event.
 */
VMM_INT_DECL(TRPMEVENT) hmSvmEventToTrpmEventType(PCSVMEVENT pEvent)
{
    uint8_t const uType = pEvent->n.u3Type;
    switch (uType)
    {
        case SVM_EVENT_EXTERNAL_IRQ:    return TRPM_HARDWARE_INT;
        case SVM_EVENT_SOFTWARE_INT:    return TRPM_SOFTWARE_INT;
        case SVM_EVENT_EXCEPTION:
        case SVM_EVENT_NMI:             return TRPM_TRAP;
        default:
            break;
    }
    AssertMsgFailed(("HMSvmEventToTrpmEvent: Invalid pending-event type %#x\n", uType));
    return TRPM_32BIT_HACK;
}


VMM_INT_DECL(bool) HMSvmNstGstIsInterruptPending(PVMCPU pVCpu, PCCPUMCTX pCtx)
{
    RT_NOREF1(pVCpu);
    PCSVMVMCBCTRL pVmcbCtrl = &pCtx->hwvirt.svm.VmcbCtrl;

    if (    !CPUMIsGuestInNestedHwVirtMode(pCtx)
        || !pCtx->hwvirt.svm.fGif)
        return false;

    X86RFLAGS RFlags;
    if (pVmcbCtrl->IntCtrl.n.u1VIntrMasking)
        RFlags.u = pCtx->rflags.u;
    else
        RFlags.u = pCtx->hwvirt.svm.HostState.rflags.u;

    if (!RFlags.Bits.u1IF)
        return false;

    return RT_BOOL(pVmcbCtrl->IntCtrl.n.u1VIrqPending);
}


VMM_INT_DECL(int) HMSvmNstGstGetInterrupt(PVMCPU pVCpu, PCCPUMCTX pCtx, uint8_t *pu8Interrupt)
{
    RT_NOREF1(pVCpu);
    PCSVMVMCBCTRL pVmcbCtrl = &pCtx->hwvirt.svm.VmcbCtrl;

    /** @todo remove later, paranoia for now. */
#ifdef DEBUG_ramshankar
    Assert(HMSvmNstGstIsInterruptPending);
#endif

    *pu8Interrupt = pVmcbCtrl->IntCtrl.n.u8VIntrVector;
    if (   pVmcbCtrl->IntCtrl.n.u1IgnoreTPR
        || pVmcbCtrl->IntCtrl.n.u4VIntrPrio > pVmcbCtrl->IntCtrl.n.u8VTPR)
        return VINF_SUCCESS;

    return VERR_APIC_INTR_MASKED_BY_TPR;
}

