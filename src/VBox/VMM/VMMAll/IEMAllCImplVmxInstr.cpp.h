/* $Id$ */
/** @file
 * IEM - VT-x instruction implementation.
 */

/*
 * Copyright (C) 2011-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/**
 * Implements 'VMCALL'.
 */
IEM_CIMPL_DEF_0(iemCImpl_vmcall)
{
    /** @todo NSTVMX: intercept. */

    /* Join forces with vmmcall. */
    return IEM_CIMPL_CALL_1(iemCImpl_Hypercall, OP_VMCALL);
}

#ifdef VBOX_WITH_NESTED_HWVIRT_VMX

/**
 * Implements VMSucceed for VMX instruction success.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 */
DECLINLINE(void) iemVmxVmSucceed(PVMCPU pVCpu)
{
    pVCpu->cpum.GstCtx.eflags.u32 &= ~(X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF);
}


/**
 * Implements VMFailInvalid for VMX instruction failure.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 */
DECLINLINE(void) iemVmxVmFailInvalid(PVMCPU pVCpu)
{
    pVCpu->cpum.GstCtx.eflags.u32 &= ~(X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF);
    pVCpu->cpum.GstCtx.eflags.u32 |= X86_EFL_CF;
}


/**
 * Implements VMFailValid for VMX instruction failure.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   enmInsErr   The VM instruction error.
 */
DECLINLINE(void) iemVmxVmFailValid(PVMCPU pVCpu, VMXINSTRERR enmInsErr)
{
    if (pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs))
    {
        pVCpu->cpum.GstCtx.eflags.u32 &= ~(X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF);
        pVCpu->cpum.GstCtx.eflags.u32 |= X86_EFL_ZF;
        /** @todo NSTVMX: VMWrite enmInsErr to VM-instruction error field. */
        RT_NOREF(enmInsErr);
    }
}


/**
 * Implements VMFail for VMX instruction failure.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   enmInsErr   The VM instruction error.
 */
DECLINLINE(void) iemVmxVmFail(PVMCPU pVCpu, VMXINSTRERR enmInsErr)
{
    if (pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs))
    {
        iemVmxVmFailValid(pVCpu, enmInsErr);
        /** @todo Set VM-instruction error field in the current virtual-VMCS.  */
    }
    else
        iemVmxVmFailInvalid(pVCpu);
}


/**
 * VMXON instruction execution worker.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   cbInstr         The instruction length.
 * @param   GCPtrVmxon      The linear address of the VMXON pointer.
 * @param   ExitInstrInfo   The VM-exit instruction information field.
 * @param   GCPtrDisp       The displacement field for @a GCPtrVmxon if any.
 *
 * @remarks Common VMX instruction checks are already expected to by the caller,
 *          i.e. CR4.VMXE, Real/V86 mode, EFER/CS.L checks.
 */
IEM_STATIC VBOXSTRICTRC iemVmxVmxon(PVMCPU pVCpu, uint8_t cbInstr, RTGCPHYS GCPtrVmxon, PCVMXEXITINSTRINFO pExitInstrInfo,
                                    RTGCPTR GCPtrDisp)
{
#if defined(VBOX_WITH_NESTED_HWVIRT_ONLY_IN_IEM) && !defined(IN_RING3)
    RT_NOREF5(pVCpu, cbInstr, GCPtrVmxon, pExitInstrInfo, GCPtrDisp);
    return VINF_EM_RAW_EMULATE_INSTR;
#else
    if (!IEM_IS_VMX_ROOT_MODE(pVCpu))
    {
        /* CPL. */
        if (pVCpu->iem.s.uCpl > 0)
        {
            Log(("vmxon: CPL %u -> #GP(0)\n", pVCpu->iem.s.uCpl));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxon_Cpl;
            return iemRaiseGeneralProtectionFault0(pVCpu);
        }

        /* A20M (A20 Masked) mode. */
        if (!PGMPhysIsA20Enabled(pVCpu))
        {
            Log(("vmxon: A20M mode -> #GP(0)\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxon_A20M;
            return iemRaiseGeneralProtectionFault0(pVCpu);
        }

        /* CR0 fixed bits. */
        bool const     fUnrestrictedGuest = IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fVmxUnrestrictedGuest;
        uint64_t const uCr0Fixed0         = fUnrestrictedGuest ? VMX_V_CR0_FIXED0_UX : VMX_V_CR0_FIXED0;
        if ((pVCpu->cpum.GstCtx.cr0 & uCr0Fixed0) != uCr0Fixed0)
        {
            Log(("vmxon: CR0 fixed0 bits cleared -> #GP(0)\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxon_Cr0Fixed0;
            return iemRaiseGeneralProtectionFault0(pVCpu);
        }

        /* CR4 fixed bits. */
        if ((pVCpu->cpum.GstCtx.cr4 & VMX_V_CR4_FIXED0) != VMX_V_CR4_FIXED0)
        {
            Log(("vmxon: CR4 fixed0 bits cleared -> #GP(0)\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxon_Cr4Fixed0;
            return iemRaiseGeneralProtectionFault0(pVCpu);
        }

        /* Feature control MSR's LOCK and VMXON bits. */
        uint64_t const uMsrFeatCtl = CPUMGetGuestIa32FeatureControl(pVCpu);
        if (!(uMsrFeatCtl & (MSR_IA32_FEATURE_CONTROL_LOCK | MSR_IA32_FEATURE_CONTROL_VMXON)))
        {
            Log(("vmxon: Feature control lock bit or VMXON bit cleared -> #GP(0)\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxon_MsrFeatCtl;
            return iemRaiseGeneralProtectionFault0(pVCpu);
        }

        /* Get the VMXON pointer from the location specified by the source memory operand. */
        RTGCPHYS GCPhysVmxon;
        VBOXSTRICTRC rcStrict = iemMemFetchDataU64(pVCpu, &GCPhysVmxon, pExitInstrInfo->InvVmxXsaves.iSegReg, GCPtrVmxon);
        if (RT_UNLIKELY(rcStrict != VINF_SUCCESS))
        {
            Log(("vmxon: Failed to read VMXON region physaddr from %#RGv, rc=%Rrc\n", GCPtrVmxon, VBOXSTRICTRC_VAL(rcStrict)));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxon_PtrMap;
            return rcStrict;
        }

        /* VMXON region pointer alignment. */
        if (GCPhysVmxon & X86_PAGE_4K_OFFSET_MASK)
        {
            Log(("vmxon: VMXON region pointer not page-aligned -> VMFailInvalid\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxon_PtrAlign;
            iemVmxVmFailInvalid(pVCpu);
            iemRegAddToRipAndClearRF(pVCpu, cbInstr);
            return VINF_SUCCESS;
        }

        /* Ensure VMXON region is not MMIO, ROM etc. This is not an Intel requirement but a
           restriction imposed by our implementation. */
        if (!PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), GCPhysVmxon))
        {
            Log(("vmxon: VMXON region not normal memory -> VMFailInvalid\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxon_PtrAbnormal;
            iemVmxVmFailInvalid(pVCpu);
            iemRegAddToRipAndClearRF(pVCpu, cbInstr);
            return VINF_SUCCESS;
        }

        /* Read the VMCS revision ID from the VMXON region. */
        VMXVMCSREVID VmcsRevId;
        int rc = PGMPhysSimpleReadGCPhys(pVCpu->CTX_SUFF(pVM), &VmcsRevId, GCPhysVmxon, sizeof(VmcsRevId));
        if (RT_FAILURE(rc))
        {
            Log(("vmxon: Failed to read VMXON region at %#RGp, rc=%Rrc\n", GCPhysVmxon, rc));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxon_PtrReadPhys;
            return rc;
        }

        /* Physical-address width. */
        uint64_t const uMsrBasic = CPUMGetGuestIa32VmxBasic(pVCpu);
        if (   RT_BF_GET(uMsrBasic, VMX_BF_BASIC_PHYSADDR_WIDTH)
            && RT_HI_U32(GCPhysVmxon))
        {
            Log(("vmxon: VMXON region pointer extends beyond physical-address width -> VMFailInvalid\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxon_PtrWidth;
            iemVmxVmFailInvalid(pVCpu);
            iemRegAddToRipAndClearRF(pVCpu, cbInstr);
            return VINF_SUCCESS;
        }

        /* Verify the VMCS revision specified by the guest matches what we reported to the guest. */
        if (RT_UNLIKELY(VmcsRevId.u != VMX_V_VMCS_REVISION_ID))
        {
            /* Revision ID mismatch. */
            if (!VmcsRevId.n.fIsShadowVmcs)
            {
                Log(("vmxon: VMCS revision mismatch, expected %#RX32 got %#RX32 -> VMFailInvalid\n", VMX_V_VMCS_REVISION_ID,
                     VmcsRevId.n.u31RevisionId));
                pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxon_VmcsRevId;
                iemVmxVmFailInvalid(pVCpu);
                iemRegAddToRipAndClearRF(pVCpu, cbInstr);
                return VINF_SUCCESS;
            }

            /* Shadow VMCS disallowed. */
            Log(("vmxon: Shadow VMCS -> VMFailInvalid\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxon_ShadowVmcs;
            iemVmxVmFailInvalid(pVCpu);
            iemRegAddToRipAndClearRF(pVCpu, cbInstr);
            return VINF_SUCCESS;
        }

        /*
         * Record that we're in VMX operation, block INIT, block and disable A20M.
         */
        pVCpu->cpum.GstCtx.hwvirt.vmx.GCPhysVmxon    = GCPhysVmxon;
        pVCpu->cpum.GstCtx.hwvirt.vmx.fInVmxRootMode = true;
        /** @todo NSTVMX: init. current VMCS pointer with ~0. */
        /** @todo NSTVMX: clear address-range monitoring. */
        /** @todo NSTVMX: Intel PT. */
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxon_Success;
        iemVmxVmSucceed(pVCpu);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
# if defined(VBOX_WITH_NESTED_HWVIRT_ONLY_IN_IEM) && defined(IN_RING3)
        return EMR3SetExecutionPolicy(pVCpu->CTX_SUFF(pVM)->pUVM, EMEXECPOLICY_IEM_ALL, true);
# else
        return VINF_SUCCESS;
# endif
    }
    else if (IEM_IS_VMX_NON_ROOT_MODE(pVCpu))
    {
        RT_NOREF(GCPtrDisp);
        /** @todo NSTVMX: intercept. */
    }

    Assert(IEM_IS_VMX_ROOT_MODE(pVCpu));

    /* CPL. */
    if (pVCpu->iem.s.uCpl > 0)
    {
        Log(("vmxon: In VMX root mode: CPL %u -> #GP(0)\n", pVCpu->iem.s.uCpl));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxon_VmxRootCpl;
        return iemRaiseGeneralProtectionFault0(pVCpu);
    }

    /* VMXON when already in VMX root mode. */
    iemVmxVmFail(pVCpu, VMXINSTRERR_VMXON_IN_VMXROOTMODE);
    pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxon_VmxRoot;
    iemRegAddToRipAndClearRF(pVCpu, cbInstr);
    return VINF_SUCCESS;
#endif
}


/**
 * Implements 'VMXON'.
 */
IEM_CIMPL_DEF_1(iemCImpl_vmxon, RTGCPTR, GCPtrVmxon)
{
    /** @todo NSTVMX: Parse ModR/M, SIB, disp.  */
    RTGCPTR GCPtrDisp = 0;
    VMXEXITINSTRINFO ExitInstrInfo;
    ExitInstrInfo.u = 0;
    ExitInstrInfo.InvVmxXsaves.u2Scaling       = 0;
    ExitInstrInfo.InvVmxXsaves.u3AddrSize      = pVCpu->iem.s.enmEffAddrMode;
    ExitInstrInfo.InvVmxXsaves.fIsRegOperand   = 0;
    ExitInstrInfo.InvVmxXsaves.iSegReg         = pVCpu->iem.s.iEffSeg;
    ExitInstrInfo.InvVmxXsaves.iIdxReg         = 0;
    ExitInstrInfo.InvVmxXsaves.fIdxRegInvalid  = 0;
    ExitInstrInfo.InvVmxXsaves.iBaseReg        = 0;
    ExitInstrInfo.InvVmxXsaves.fBaseRegInvalid = 0;
    ExitInstrInfo.InvVmxXsaves.iReg2           = 0;
    return iemVmxVmxon(pVCpu, cbInstr, GCPtrVmxon, &ExitInstrInfo, GCPtrDisp);
}


/**
 * Implements 'VMXOFF'.
 */
IEM_CIMPL_DEF_0(iemCImpl_vmxoff)
{
# if defined(VBOX_WITH_NESTED_HWVIRT_ONLY_IN_IEM) && !defined(IN_RING3)
    RT_NOREF2(pVCpu, cbInstr);
    return VINF_EM_RAW_EMULATE_INSTR;
# else
    IEM_VMX_INSTR_COMMON_CHECKS(pVCpu, "vmxoff", kVmxVInstrDiag_Vmxoff);
    if (!IEM_IS_VMX_ROOT_MODE(pVCpu))
    {
        Log(("vmxoff: Not in VMX root mode -> #GP(0)\n"));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxoff_VmxRoot;
        return iemRaiseUndefinedOpcode(pVCpu);
    }

    if (IEM_IS_VMX_NON_ROOT_MODE(pVCpu))
    {
        /** @todo NSTVMX: intercept. */
    }

    /* CPL. */
    if (pVCpu->iem.s.uCpl > 0)
    {
        Log(("vmxoff: CPL %u -> #GP(0)\n", pVCpu->iem.s.uCpl));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxoff_Cpl;
        return iemRaiseGeneralProtectionFault0(pVCpu);
    }

    /* Dual monitor treatment of SMIs and SMM. */
    uint64_t const fSmmMonitorCtl = CPUMGetGuestIa32SmmMonitorCtl(pVCpu);
    if (fSmmMonitorCtl & MSR_IA32_SMM_MONITOR_VALID)
    {
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMXOFF_DUAL_MON);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /*
     * Record that we're no longer in VMX root operation, block INIT, block and disable A20M.
     */
    pVCpu->cpum.GstCtx.hwvirt.vmx.fInVmxRootMode = false;
    Assert(!pVCpu->cpum.GstCtx.hwvirt.vmx.fInVmxNonRootMode);

    if (fSmmMonitorCtl & MSR_IA32_SMM_MONITOR_VMXOFF_UNBLOCK_SMI)
    { /** @todo NSTVMX: Unblock SMI. */ }
    /** @todo NSTVMX: Unblock and enable A20M. */
    /** @todo NSTVMX: Clear address-range monitoring. */

    pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxoff_Success;
    iemVmxVmSucceed(pVCpu);
    iemRegAddToRipAndClearRF(pVCpu, cbInstr);
#  if defined(VBOX_WITH_NESTED_HWVIRT_ONLY_IN_IEM) && defined(IN_RING3)
    return EMR3SetExecutionPolicy(pVCpu->CTX_SUFF(pVM)->pUVM, EMEXECPOLICY_IEM_ALL, false);
#  else
    return VINF_SUCCESS;
#  endif
# endif
}

#endif

