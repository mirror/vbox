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
#define VMCPU_INCL_CPUM_GST_CTX
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


#ifndef IN_RC
/**
 * Converts an IEM exception event type to an SVM event type.
 *
 * @returns The SVM event type.
 * @retval  UINT8_MAX if the specified type of event isn't among the set
 *          of recognized IEM event types.
 *
 * @param   uVector         The vector of the event.
 * @param   fIemXcptFlags   The IEM exception / interrupt flags.
 */
static uint8_t hmSvmEventTypeFromIemEvent(uint32_t uVector, uint32_t fIemXcptFlags)
{
    if (fIemXcptFlags & IEM_XCPT_FLAGS_T_CPU_XCPT)
        return SVM_EVENT_EXCEPTION;
    if (fIemXcptFlags & IEM_XCPT_FLAGS_T_EXT_INT)
        return uVector != X86_XCPT_NMI ? SVM_EVENT_EXTERNAL_IRQ : SVM_EVENT_NMI;
    if (fIemXcptFlags & IEM_XCPT_FLAGS_T_SOFT_INT)
        return SVM_EVENT_SOFTWARE_INT;
    AssertMsgFailed(("hmSvmEventTypeFromIemEvent: Invalid IEM xcpt/int. type %#x, uVector=%#x\n", fIemXcptFlags, uVector));
    return UINT8_MAX;
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
                && !pVM->cpum.ro.GuestFeatures.fSvmNestedPaging)
            {
                Log(("HMSvmVmRun: Nested paging not supported -> #VMEXIT\n"));
                return HMSvmNstGstVmExit(pVCpu, pCtx, SVM_EXIT_INVALID, 0 /* uExitInfo1 */, 0 /* uExitInfo2 */);
            }

            /* AVIC. */
            if (    pVmcbCtrl->IntCtrl.n.u1AvicEnable
                && !pVM->cpum.ro.GuestFeatures.fSvmAvic)
            {
                Log(("HMSvmVmRun: AVIC not supported -> #VMEXIT\n"));
                return HMSvmNstGstVmExit(pVCpu, pCtx, SVM_EXIT_INVALID, 0 /* uExitInfo1 */, 0 /* uExitInfo2 */);
            }

            /* Last branch record (LBR) virtualization. */
            if (    (pVmcbCtrl->u64LBRVirt & SVM_LBR_VIRT_ENABLE)
                && !pVM->cpum.ro.GuestFeatures.fSvmLbrVirt)
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
            rc = CPUMQueryValidatedGuestEfer(pVM, VmcbNstGst.u64CR0, 0 /* uOldEfer */, VmcbNstGst.u64EFER, &uValidEfer);
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
                if (   rcStrict == VINF_SVM_VMEXIT
                    || rcStrict == VERR_SVM_VMEXIT_FAILED)
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

        /*
         * Disable the global interrupt flag to prevent interrupts during the 'atomic' world switch.
         */
        pCtx->hwvirt.svm.fGif = 0;

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
            pCtx->hwvirt.svm.VmcbCtrl.u64IntShadow |= SVM_INTERRUPT_SHADOW_ACTIVE;

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
         * Update the exit interrupt information field if this #VMEXIT happened as a result
         * of delivering an event.
         */
        {
            uint8_t  uExitIntVector;
            uint32_t uExitIntErr;
            uint32_t fExitIntFlags;
            bool const fRaisingEvent = IEMGetCurrentXcpt(pVCpu, &uExitIntVector, &fExitIntFlags, &uExitIntErr,
                                                         NULL /* uExitIntCr2 */);
            pCtx->hwvirt.svm.VmcbCtrl.ExitIntInfo.n.u1Valid = fRaisingEvent;
            if (fRaisingEvent)
            {
                pCtx->hwvirt.svm.VmcbCtrl.ExitIntInfo.n.u8Vector = uExitIntVector;
                pCtx->hwvirt.svm.VmcbCtrl.ExitIntInfo.n.u3Type   = hmSvmEventTypeFromIemEvent(uExitIntVector, fExitIntFlags);
                if (fExitIntFlags & IEM_XCPT_FLAGS_ERR)
                {
                    pCtx->hwvirt.svm.VmcbCtrl.ExitIntInfo.n.u1ErrorCodeValid = true;
                    pCtx->hwvirt.svm.VmcbCtrl.ExitIntInfo.n.u32ErrorCode     = uExitIntErr;
                }
            }
        }

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
            pCtx->dr[7]     &= ~(X86_DR7_ENABLED_MASK | X86_DR7_RAZ_MASK | X86_DR7_MBZ_MASK);
            pCtx->dr[7]     |= X86_DR7_RA1_MASK;

            /** @todo if RIP is not canonical or outside the CS segment limit, we need to
             *        raise \#GP(0) in the guest. */

            /** @todo check the loaded host-state for consistency. Figure out what
             *        exactly this involves? */

            rc = VINF_SVM_VMEXIT;
        }
        else
        {
            Log(("HMNstGstSvmVmExit: Writing VMCB at %#RGp failed\n", pCtx->hwvirt.svm.GCPhysVmcb));
            Assert(!CPUMIsGuestInNestedHwVirtMode(pCtx));
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
 * Checks whether an interrupt is pending for the nested-guest.
 *
 * @returns VBox status code.
 * @retval  true if there's a pending interrupt, false otherwise.
 *
 * @param   pCtx            The guest-CPU context.
 */
VMM_INT_DECL(bool) HMSvmNstGstIsInterruptPending(PCCPUMCTX pCtx)
{
    PCSVMVMCBCTRL pVmcbCtrl = &pCtx->hwvirt.svm.VmcbCtrl;
    if (!CPUMIsGuestInNestedHwVirtMode(pCtx))
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


/**
 * Gets the pending nested-guest interrupt.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_APIC_INTR_MASKED_BY_TPR when an APIC interrupt is pending but
 *          can't be delivered due to TPR priority.
 * @retval  VERR_NO_DATA if there is no interrupt to be delivered (either APIC
 *          has been software-disabled since it flagged something was pending,
 *          or other reasons).
 *
 * @param   pCtx            The guest-CPU context.
 * @param   pu8Interrupt    Where to store the interrupt.
 */
VMM_INT_DECL(int) HMSvmNstGstGetInterrupt(PCCPUMCTX pCtx, uint8_t *pu8Interrupt)
{
    PCSVMVMCBCTRL pVmcbCtrl = &pCtx->hwvirt.svm.VmcbCtrl;
    /** @todo remove later, paranoia for now. */
#ifdef DEBUG_ramshankar
    Assert(HMSvmNstGstIsInterruptPending(pCtx));
#endif

    *pu8Interrupt = pVmcbCtrl->IntCtrl.n.u8VIntrVector;
    if (   pVmcbCtrl->IntCtrl.n.u1IgnoreTPR
        || pVmcbCtrl->IntCtrl.n.u4VIntrPrio > pVmcbCtrl->IntCtrl.n.u8VTPR)
        return VINF_SUCCESS;

    return VERR_APIC_INTR_MASKED_BY_TPR;
}


/**
 * Handles nested-guest SVM control intercepts and performs the \#VMEXIT if the
 * intercept is active.
 *
 * @returns Strict VBox status code.
 * @retval  VINF_SVM_INTERCEPT_NOT_ACTIVE if the intercept is not active or
 *          we're not executing a nested-guest.
 * @retval  VINF_SVM_VMEXIT if the intercept is active and the \#VMEXIT occurred
 *          successfully.
 * @retval  VERR_SVM_VMEXIT_FAILED if the intercept is active and the \#VMEXIT
 *          failed and a shutdown needs to be initiated for the geust.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pCtx        The guest-CPU context.
 * @param   uExitCode   The SVM exit code (see SVM_EXIT_XXX).
 * @param   uExitInfo1  The exit info. 1 field.
 * @param   uExitInfo2  The exit info. 2 field.
 */
VMM_INT_DECL(VBOXSTRICTRC) HMSvmNstGstHandleCtrlIntercept(PVMCPU pVCpu, PCPUMCTX pCtx, uint64_t uExitCode, uint64_t uExitInfo1,
                                                          uint64_t uExitInfo2)
{
#define HMSVM_CTRL_INTERCEPT_VMEXIT(a_Intercept) \
    do { \
        if (CPUMIsGuestSvmCtrlInterceptSet(pCtx, (a_Intercept))) \
            return HMSvmNstGstVmExit(pVCpu, pCtx, uExitCode, uExitInfo1, uExitInfo2); \
        break; \
    } while (0)

    if (!CPUMIsGuestInNestedHwVirtMode(pCtx))
        return VINF_HM_INTERCEPT_NOT_ACTIVE;

    switch (uExitCode)
    {
        case SVM_EXIT_EXCEPTION_0:  case SVM_EXIT_EXCEPTION_1:  case SVM_EXIT_EXCEPTION_2:  case SVM_EXIT_EXCEPTION_3:
        case SVM_EXIT_EXCEPTION_4:  case SVM_EXIT_EXCEPTION_5:  case SVM_EXIT_EXCEPTION_6:  case SVM_EXIT_EXCEPTION_7:
        case SVM_EXIT_EXCEPTION_8:  case SVM_EXIT_EXCEPTION_9:  case SVM_EXIT_EXCEPTION_10: case SVM_EXIT_EXCEPTION_11:
        case SVM_EXIT_EXCEPTION_12: case SVM_EXIT_EXCEPTION_13: case SVM_EXIT_EXCEPTION_14: case SVM_EXIT_EXCEPTION_15:
        case SVM_EXIT_EXCEPTION_16: case SVM_EXIT_EXCEPTION_17: case SVM_EXIT_EXCEPTION_18: case SVM_EXIT_EXCEPTION_19:
        case SVM_EXIT_EXCEPTION_20: case SVM_EXIT_EXCEPTION_21: case SVM_EXIT_EXCEPTION_22: case SVM_EXIT_EXCEPTION_23:
        case SVM_EXIT_EXCEPTION_24: case SVM_EXIT_EXCEPTION_25: case SVM_EXIT_EXCEPTION_26: case SVM_EXIT_EXCEPTION_27:
        case SVM_EXIT_EXCEPTION_28: case SVM_EXIT_EXCEPTION_29: case SVM_EXIT_EXCEPTION_30: case SVM_EXIT_EXCEPTION_31:
        {
            if (CPUMIsGuestSvmXcptInterceptSet(pCtx, (X86XCPT)(uExitCode - SVM_EXIT_EXCEPTION_0)))
                    return HMSvmNstGstVmExit(pVCpu, pCtx, uExitCode, uExitInfo1, uExitInfo2);
                break;
        }

        case SVM_EXIT_WRITE_CR0:  case SVM_EXIT_WRITE_CR1:  case SVM_EXIT_WRITE_CR2:  case SVM_EXIT_WRITE_CR3:
        case SVM_EXIT_WRITE_CR4:  case SVM_EXIT_WRITE_CR5:  case SVM_EXIT_WRITE_CR6:  case SVM_EXIT_WRITE_CR7:
        case SVM_EXIT_WRITE_CR8:  case SVM_EXIT_WRITE_CR9:  case SVM_EXIT_WRITE_CR10: case SVM_EXIT_WRITE_CR11:
        case SVM_EXIT_WRITE_CR12: case SVM_EXIT_WRITE_CR13: case SVM_EXIT_WRITE_CR14: case SVM_EXIT_WRITE_CR15:
        {
            if (CPUMIsGuestSvmWriteCRxInterceptSet(pCtx, uExitCode - SVM_EXIT_WRITE_CR0))
                return HMSvmNstGstVmExit(pVCpu, pCtx, uExitCode, uExitInfo1, uExitInfo2);
            break;
        }

        case SVM_EXIT_READ_CR0:  case SVM_EXIT_READ_CR1:  case SVM_EXIT_READ_CR2:  case SVM_EXIT_READ_CR3:
        case SVM_EXIT_READ_CR4:  case SVM_EXIT_READ_CR5:  case SVM_EXIT_READ_CR6:  case SVM_EXIT_READ_CR7:
        case SVM_EXIT_READ_CR8:  case SVM_EXIT_READ_CR9:  case SVM_EXIT_READ_CR10: case SVM_EXIT_READ_CR11:
        case SVM_EXIT_READ_CR12: case SVM_EXIT_READ_CR13: case SVM_EXIT_READ_CR14: case SVM_EXIT_READ_CR15:
        {
            if (CPUMIsGuestSvmReadCRxInterceptSet(pCtx, uExitCode - SVM_EXIT_READ_CR0))
                return HMSvmNstGstVmExit(pVCpu, pCtx, uExitCode, uExitInfo1, uExitInfo2);
            break;
        }

        case SVM_EXIT_READ_DR0:  case SVM_EXIT_READ_DR1:  case SVM_EXIT_READ_DR2:  case SVM_EXIT_READ_DR3:
        case SVM_EXIT_READ_DR4:  case SVM_EXIT_READ_DR5:  case SVM_EXIT_READ_DR6:  case SVM_EXIT_READ_DR7:
        case SVM_EXIT_READ_DR8:  case SVM_EXIT_READ_DR9:  case SVM_EXIT_READ_DR10: case SVM_EXIT_READ_DR11:
        case SVM_EXIT_READ_DR12: case SVM_EXIT_READ_DR13: case SVM_EXIT_READ_DR14: case SVM_EXIT_READ_DR15:
        {
            if (CPUMIsGuestSvmReadDRxInterceptSet(pCtx, uExitCode - SVM_EXIT_READ_DR0))
                return HMSvmNstGstVmExit(pVCpu, pCtx, uExitCode, uExitInfo1, uExitInfo2);
            break;
        }

        case SVM_EXIT_WRITE_DR0:  case SVM_EXIT_WRITE_DR1:  case SVM_EXIT_WRITE_DR2:  case SVM_EXIT_WRITE_DR3:
        case SVM_EXIT_WRITE_DR4:  case SVM_EXIT_WRITE_DR5:  case SVM_EXIT_WRITE_DR6:  case SVM_EXIT_WRITE_DR7:
        case SVM_EXIT_WRITE_DR8:  case SVM_EXIT_WRITE_DR9:  case SVM_EXIT_WRITE_DR10: case SVM_EXIT_WRITE_DR11:
        case SVM_EXIT_WRITE_DR12: case SVM_EXIT_WRITE_DR13: case SVM_EXIT_WRITE_DR14: case SVM_EXIT_WRITE_DR15:
        {
            if (CPUMIsGuestSvmWriteDRxInterceptSet(pCtx, uExitCode - SVM_EXIT_WRITE_DR0))
                return HMSvmNstGstVmExit(pVCpu, pCtx, uExitCode, uExitInfo1, uExitInfo2);
            break;
        }

        case SVM_EXIT_INTR:                HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_INTR);
        case SVM_EXIT_NMI:                 HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_NMI);
        case SVM_EXIT_SMI:                 HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_SMI);
        case SVM_EXIT_INIT:                HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_INIT);
        case SVM_EXIT_VINTR:               HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_VINTR);
        case SVM_EXIT_CR0_SEL_WRITE:       HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_CR0_SEL_WRITES);
        case SVM_EXIT_IDTR_READ:           HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_IDTR_READS);
        case SVM_EXIT_GDTR_READ:           HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_GDTR_READS);
        case SVM_EXIT_LDTR_READ:           HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_LDTR_READS);
        case SVM_EXIT_TR_READ:             HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_TR_READS);
        case SVM_EXIT_IDTR_WRITE:          HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_IDTR_WRITES);
        case SVM_EXIT_GDTR_WRITE:          HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_GDTR_WRITES);
        case SVM_EXIT_LDTR_WRITE:          HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_LDTR_WRITES);
        case SVM_EXIT_TR_WRITE:            HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_TR_WRITES);
        case SVM_EXIT_RDTSC:               HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_RDTSC);
        case SVM_EXIT_RDPMC:               HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_RDPMC);
        case SVM_EXIT_PUSHF:               HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_PUSHF);
        case SVM_EXIT_POPF:                HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_POPF);
        case SVM_EXIT_CPUID:               HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_CPUID);
        case SVM_EXIT_RSM:                 HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_RSM);
        case SVM_EXIT_IRET:                HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_IRET);
        case SVM_EXIT_SWINT:               HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_INTN);
        case SVM_EXIT_INVD:                HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_INVD);
        case SVM_EXIT_PAUSE:               HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_PAUSE);
        case SVM_EXIT_HLT:                 HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_HLT);
        case SVM_EXIT_INVLPG:              HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_INVLPG);
        case SVM_EXIT_INVLPGA:             HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_INVLPGA);
        case SVM_EXIT_TASK_SWITCH:         HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_TASK_SWITCH);
        case SVM_EXIT_FERR_FREEZE:         HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_FERR_FREEZE);
        case SVM_EXIT_SHUTDOWN:            HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_SHUTDOWN);
        case SVM_EXIT_VMRUN:               HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_VMRUN);
        case SVM_EXIT_VMMCALL:             HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_VMMCALL);
        case SVM_EXIT_VMLOAD:              HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_VMLOAD);
        case SVM_EXIT_VMSAVE:              HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_VMSAVE);
        case SVM_EXIT_STGI:                HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_STGI);
        case SVM_EXIT_CLGI:                HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_CLGI);
        case SVM_EXIT_SKINIT:              HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_SKINIT);
        case SVM_EXIT_RDTSCP:              HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_RDTSCP);
        case SVM_EXIT_ICEBP:               HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_ICEBP);
        case SVM_EXIT_WBINVD:              HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_WBINVD);
        case SVM_EXIT_MONITOR:             HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_MONITOR);
        case SVM_EXIT_MWAIT:               HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_MWAIT);
        case SVM_EXIT_MWAIT_ARMED:         HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_MWAIT_ARMED);
        case SVM_EXIT_XSETBV:              HMSVM_CTRL_INTERCEPT_VMEXIT(SVM_CTRL_INTERCEPT_XSETBV);

        case SVM_EXIT_IOIO:
            AssertMsgFailed(("Use HMSvmNstGstHandleMsrIntercept!\n"));
            return VERR_SVM_IPE_1;

        case SVM_EXIT_MSR:
            AssertMsgFailed(("Use HMSvmNstGstHandleMsrIntercept!\n"));
            return VERR_SVM_IPE_1;

        case SVM_EXIT_NPF:
        case SVM_EXIT_AVIC_INCOMPLETE_IPI:
        case SVM_EXIT_AVIC_NOACCEL:
            AssertMsgFailed(("Todo Implement.\n"));
            return VERR_SVM_IPE_1;

        default:
            AssertMsgFailed(("Unsupported SVM exit code %#RX64\n", uExitCode));
            return VERR_SVM_IPE_1;
    }

    return VINF_HM_INTERCEPT_NOT_ACTIVE;

#undef HMSVM_CTRL_INTERCEPT_VMEXIT
}


/**
 * Handles nested-guest SVM IO intercepts and performs the \#VMEXIT
 * if the intercept is active.
 *
 * @returns Strict VBox status code.
 * @retval  VINF_SVM_INTERCEPT_NOT_ACTIVE if the intercept is not active or
 *          we're not executing a nested-guest.
 * @retval  VINF_SVM_VMEXIT if the intercept is active and the \#VMEXIT occurred
 *          successfully.
 * @retval  VERR_SVM_VMEXIT_FAILED if the intercept is active and the \#VMEXIT
 *          failed and a shutdown needs to be initiated for the geust.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pCtx            The guest-CPU context.
 * @param   pIoExitInfo     The SVM IOIO exit info. structure.
 * @param   uNextRip        The RIP of the instruction following the IO
 *                          instruction.
 */
VMM_INT_DECL(VBOXSTRICTRC) HMSvmNstGstHandleIOIntercept(PVMCPU pVCpu, PCPUMCTX pCtx, PCSVMIOIOEXITINFO pIoExitInfo,
                                                        uint64_t uNextRip)
{
    /*
     * Check if any IO accesses are being intercepted.
     */
    Assert(CPUMIsGuestInNestedHwVirtMode(pCtx));
    Assert(CPUMIsGuestSvmCtrlInterceptSet(pCtx, SVM_CTRL_INTERCEPT_IOIO_PROT));

    /*
     * The IOPM layout:
     * Each bit represents one 8-bit port. That makes a total of 0..65535 bits or
     * two 4K pages.
     *
     * For IO instructions that access more than a single byte, the permission bits
     * for all bytes are checked; if any bit is set to 1, the IO access is intercepted.
     *
     * Since it's possible to do a 32-bit IO access at port 65534 (accessing 4 bytes),
     * we need 3 extra bits beyond the second 4K page.
     */
    uint8_t const *pbIopm = (uint8_t *)pCtx->hwvirt.svm.CTX_SUFF(pvIoBitmap);

    uint16_t const u16Port   = pIoExitInfo->n.u16Port;
    uint16_t const offIopm   = u16Port >> 3;
    uint16_t const fSizeMask = pIoExitInfo->n.u1OP32 ? 0xf : pIoExitInfo->n.u1OP16 ? 3 : 1;
    uint8_t  const cShift    = u16Port - (offIopm << 3);
    uint16_t const fIopmMask = (1 << cShift) | (fSizeMask << cShift);

    pbIopm += offIopm;
    uint16_t const fIopmBits = *(uint16_t *)pbIopm;
    if (fIopmBits & fIopmMask)
        return HMSvmNstGstVmExit(pVCpu, pCtx, SVM_EXIT_IOIO, pIoExitInfo->u, uNextRip);

    return VINF_HM_INTERCEPT_NOT_ACTIVE;
}


/**
 * Handles nested-guest SVM MSR read/write intercepts and performs the \#VMEXIT
 * if the intercept is active.
 *
 * @returns Strict VBox status code.
 * @retval  VINF_SVM_INTERCEPT_NOT_ACTIVE if the MSR permission bitmap does not
 *          specify interception of the accessed MSR @a idMsr.
 * @retval  VINF_SVM_VMEXIT if the intercept is active and the \#VMEXIT occurred
 *          successfully.
 * @retval  VERR_SVM_VMEXIT_FAILED if the intercept is active and the \#VMEXIT
 *          failed and a shutdown needs to be initiated for the geust.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pCtx        The guest-CPU context.
 * @param   idMsr       The MSR being accessed in the nested-guest.
 * @param   fWrite      Whether this is an MSR write access, @c false implies an
 *                      MSR read.
 */
VMM_INT_DECL(VBOXSTRICTRC) HMSvmNstGstHandleMsrIntercept(PVMCPU pVCpu, PCPUMCTX pCtx, uint32_t idMsr, bool fWrite)
{
    /*
     * Check if any MSRs are being intercepted.
     */
    Assert(CPUMIsGuestSvmCtrlInterceptSet(pCtx, SVM_CTRL_INTERCEPT_MSR_PROT));
    Assert(CPUMIsGuestInNestedHwVirtMode(pCtx));

    uint64_t const uExitInfo1 = fWrite ? SVM_EXIT1_MSR_WRITE : SVM_EXIT1_MSR_READ;

    /*
     * Get the byte and bit offset of the permission bits corresponding to the MSR.
     */
    uint16_t offMsrpm;
    uint32_t uMsrpmBit;
    int rc = hmSvmGetMsrpmOffsetAndBit(idMsr, &offMsrpm, &uMsrpmBit);
    if (RT_SUCCESS(rc))
    {
        Assert(uMsrpmBit < 0x3fff);
        Assert(offMsrpm < SVM_MSRPM_PAGES << X86_PAGE_4K_SHIFT);
        if (fWrite)
            ++uMsrpmBit;

        /*
         * Check if the bit is set, if so, trigger a #VMEXIT.
         */
        uint8_t *pbMsrpm = (uint8_t *)pCtx->hwvirt.svm.CTX_SUFF(pvMsrBitmap);
        pbMsrpm += offMsrpm;
        if (ASMBitTest(pbMsrpm, uMsrpmBit))
            return HMSvmNstGstVmExit(pVCpu, pCtx, SVM_EXIT_MSR, uExitInfo1, 0 /* uExitInfo2 */);
    }
    else
    {
        /*
         * This shouldn't happen, but if it does, cause a #VMEXIT and let the "host" (guest hypervisor) deal with it.
         */
        Log(("HMSvmNstGstHandleIntercept: Invalid/out-of-range MSR %#RX32 fWrite=%RTbool\n", idMsr, fWrite));
        return HMSvmNstGstVmExit(pVCpu, pCtx, SVM_EXIT_MSR, uExitInfo1, 0 /* uExitInfo2 */);
    }
    return VINF_HM_INTERCEPT_NOT_ACTIVE;
}


/**
 * Gets the MSR permission bitmap byte and bit offset for the specified MSR.
 *
 * @returns VBox status code.
 * @param   idMsr       The MSR being requested.
 * @param   pbOffMsrpm  Where to store the byte offset in the MSR permission
 *                      bitmap for @a idMsr.
 * @param   puMsrpmBit  Where to store the bit offset starting at the byte
 *                      returned in @a pbOffMsrpm.
 */
VMM_INT_DECL(int) hmSvmGetMsrpmOffsetAndBit(uint32_t idMsr, uint16_t *pbOffMsrpm, uint32_t *puMsrpmBit)
{
    Assert(pbOffMsrpm);
    Assert(puMsrpmBit);

    /*
     * MSRPM Layout:
     * Byte offset          MSR range
     * 0x000  - 0x7ff       0x00000000 - 0x00001fff
     * 0x800  - 0xfff       0xc0000000 - 0xc0001fff
     * 0x1000 - 0x17ff      0xc0010000 - 0xc0011fff
     * 0x1800 - 0x1fff              Reserved
     *
     * Each MSR is represented by 2 permission bits (read and write).
     */
    if (idMsr <= 0x00001fff)
    {
        /* Pentium-compatible MSRs. */
        *pbOffMsrpm = 0;
        *puMsrpmBit = idMsr << 1;
        return VINF_SUCCESS;
    }

    if (   idMsr >= 0xc0000000
        && idMsr <= 0xc0001fff)
    {
        /* AMD Sixth Generation x86 Processor MSRs. */
        *pbOffMsrpm = 0x800;
        *puMsrpmBit = (idMsr - 0xc0000000) << 1;
        return VINF_SUCCESS;
    }

    if (   idMsr >= 0xc0010000
        && idMsr <= 0xc0011fff)
    {
        /* AMD Seventh and Eighth Generation Processor MSRs. */
        *pbOffMsrpm += 0x1000;
        *puMsrpmBit = (idMsr - 0xc0001000) << 1;
        return VINF_SUCCESS;
    }

    *pbOffMsrpm = 0;
    *puMsrpmBit = 0;
    return VERR_OUT_OF_RANGE;
}
#endif /* !IN_RC */

