/* $Id$ */
/** @file
 * NEM - Native execution manager, Windows code template ring-0/3.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
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
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Copy back a segment from hyper-V. */
#define NEM_WIN_COPY_BACK_SEG(a_Dst, a_Src) \
            do { \
                (a_Dst).u64Base  = (a_Src).Base; \
                (a_Dst).u32Limit = (a_Src).Limit; \
                (a_Dst).ValidSel = (a_Dst).Sel = (a_Src).Selector; \
                (a_Dst).Attr.u   = (a_Src).Attributes; \
                (a_Dst).fFlags   = CPUMSELREG_FLAGS_VALID; \
            } while (0)


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** NEM_WIN_PAGE_STATE_XXX names. */
NEM_TMPL_STATIC const char * const g_apszPageStates[4] = { "not-set", "unmapped", "readable", "writable" };

/** HV_INTERCEPT_ACCESS_TYPE names. */
static const char * const g_apszHvInterceptAccessTypes[4] = { "read", "write", "exec", "!undefined!" };


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
NEM_TMPL_STATIC int nemHCNativeSetPhysPage(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhysSrc, RTGCPHYS GCPhysDst,
                                           uint32_t fPageProt, uint8_t *pu2State, bool fBackingChanged);


#ifdef NEM_WIN_USE_HYPERCALLS_FOR_PAGES

/**
 * Wrapper around VMMR0_DO_NEM_MAP_PAGES for a single page.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure of the caller.
 * @param   GCPhysSrc   The source page.  Does not need to be page aligned.
 * @param   GCPhysDst   The destination page.  Same as @a GCPhysSrc except for
 *                      when A20 is disabled.
 * @param   fFlags      HV_MAP_GPA_XXX.
 */
DECLINLINE(int) nemHCWinHypercallMapPage(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhysSrc, RTGCPHYS GCPhysDst, uint32_t fFlags)
{
#ifdef IN_RING0
    /** @todo optimize further, caller generally has the physical address. */
    PGVM pGVM = GVMMR0FastGetGVMByVM(pVM);
    AssertReturn(pGVM, VERR_INVALID_VM_HANDLE);
    return nemR0WinMapPages(pGVM, pVM, &pGVM->aCpus[pVCpu->idCpu],
                            GCPhysSrc & ~(RTGCPHYS)X86_PAGE_OFFSET_MASK,
                            GCPhysDst & ~(RTGCPHYS)X86_PAGE_OFFSET_MASK,
                            1, fFlags);
#else
    pVCpu->nem.s.Hypercall.MapPages.GCPhysSrc   = GCPhysSrc & ~(RTGCPHYS)X86_PAGE_OFFSET_MASK;
    pVCpu->nem.s.Hypercall.MapPages.GCPhysDst   = GCPhysDst & ~(RTGCPHYS)X86_PAGE_OFFSET_MASK;
    pVCpu->nem.s.Hypercall.MapPages.cPages      = 1;
    pVCpu->nem.s.Hypercall.MapPages.fFlags      = fFlags;
    return VMMR3CallR0Emt(pVM, pVCpu, VMMR0_DO_NEM_MAP_PAGES, 0, NULL);
#endif
}


/**
 * Wrapper around VMMR0_DO_NEM_UNMAP_PAGES for a single page.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure of the caller.
 * @param   GCPhys      The page to unmap.  Does not need to be page aligned.
 */
DECLINLINE(int) nemHCWinHypercallUnmapPage(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhys)
{
# ifdef IN_RING0
    PGVM pGVM = GVMMR0FastGetGVMByVM(pVM);
    AssertReturn(pGVM, VERR_INVALID_VM_HANDLE);
    return nemR0WinUnmapPages(pGVM, &pGVM->aCpus[pVCpu->idCpu], GCPhys & ~(RTGCPHYS)X86_PAGE_OFFSET_MASK, 1);
# else
    pVCpu->nem.s.Hypercall.UnmapPages.GCPhys    = GCPhys & ~(RTGCPHYS)X86_PAGE_OFFSET_MASK;
    pVCpu->nem.s.Hypercall.UnmapPages.cPages    = 1;
    return VMMR3CallR0Emt(pVM, pVCpu, VMMR0_DO_NEM_UNMAP_PAGES, 0, NULL);
# endif
}

#endif /* NEM_WIN_USE_HYPERCALLS_FOR_PAGES */
#ifndef IN_RING0

NEM_TMPL_STATIC int nemHCWinCopyStateToHyperV(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
# ifdef NEM_WIN_USE_HYPERCALLS_FOR_REGISTERS
    NOREF(pCtx);
    int rc = VMMR3CallR0Emt(pVM, pVCpu, VMMR0_DO_NEM_EXPORT_STATE, 0, NULL);
    AssertLogRelRCReturn(rc, rc);
    return rc;

# else
    WHV_REGISTER_NAME  aenmNames[128];
    WHV_REGISTER_VALUE aValues[128];

    /* GPRs */
    aenmNames[0]      = WHvX64RegisterRax;
    aValues[0].Reg64  = pCtx->rax;
    aenmNames[1]      = WHvX64RegisterRcx;
    aValues[1].Reg64  = pCtx->rcx;
    aenmNames[2]      = WHvX64RegisterRdx;
    aValues[2].Reg64  = pCtx->rdx;
    aenmNames[3]      = WHvX64RegisterRbx;
    aValues[3].Reg64  = pCtx->rbx;
    aenmNames[4]      = WHvX64RegisterRsp;
    aValues[4].Reg64  = pCtx->rsp;
    aenmNames[5]      = WHvX64RegisterRbp;
    aValues[5].Reg64  = pCtx->rbp;
    aenmNames[6]      = WHvX64RegisterRsi;
    aValues[6].Reg64  = pCtx->rsi;
    aenmNames[7]      = WHvX64RegisterRdi;
    aValues[7].Reg64  = pCtx->rdi;
    aenmNames[8]      = WHvX64RegisterR8;
    aValues[8].Reg64  = pCtx->r8;
    aenmNames[9]      = WHvX64RegisterR9;
    aValues[9].Reg64  = pCtx->r9;
    aenmNames[10]     = WHvX64RegisterR10;
    aValues[10].Reg64 = pCtx->r10;
    aenmNames[11]     = WHvX64RegisterR11;
    aValues[11].Reg64 = pCtx->r11;
    aenmNames[12]     = WHvX64RegisterR12;
    aValues[12].Reg64 = pCtx->r12;
    aenmNames[13]     = WHvX64RegisterR13;
    aValues[13].Reg64 = pCtx->r13;
    aenmNames[14]     = WHvX64RegisterR14;
    aValues[14].Reg64 = pCtx->r14;
    aenmNames[15]     = WHvX64RegisterR15;
    aValues[15].Reg64 = pCtx->r15;

    /* RIP & Flags */
    aenmNames[16]     = WHvX64RegisterRip;
    aValues[16].Reg64 = pCtx->rip;
    aenmNames[17]     = WHvX64RegisterRflags;
    aValues[17].Reg64 = pCtx->rflags.u;

    /* Segments */
#  define COPY_OUT_SEG(a_idx, a_enmName, a_SReg) \
        do { \
            aenmNames[a_idx]                  = a_enmName; \
            aValues[a_idx].Segment.Base       = (a_SReg).u64Base; \
            aValues[a_idx].Segment.Limit      = (a_SReg).u32Limit; \
            aValues[a_idx].Segment.Selector   = (a_SReg).Sel; \
            aValues[a_idx].Segment.Attributes = (a_SReg).Attr.u; \
        } while (0)
    COPY_OUT_SEG(18, WHvX64RegisterEs,   pCtx->es);
    COPY_OUT_SEG(19, WHvX64RegisterCs,   pCtx->cs);
    COPY_OUT_SEG(20, WHvX64RegisterSs,   pCtx->ss);
    COPY_OUT_SEG(21, WHvX64RegisterDs,   pCtx->ds);
    COPY_OUT_SEG(22, WHvX64RegisterFs,   pCtx->fs);
    COPY_OUT_SEG(23, WHvX64RegisterGs,   pCtx->gs);
    COPY_OUT_SEG(24, WHvX64RegisterLdtr, pCtx->ldtr);
    COPY_OUT_SEG(25, WHvX64RegisterTr,   pCtx->tr);

    uintptr_t iReg = 26;
    /* Descriptor tables. */
    aenmNames[iReg] = WHvX64RegisterIdtr;
    aValues[iReg].Table.Limit = pCtx->idtr.cbIdt;
    aValues[iReg].Table.Base  = pCtx->idtr.pIdt;
    iReg++;
    aenmNames[iReg] = WHvX64RegisterGdtr;
    aValues[iReg].Table.Limit = pCtx->gdtr.cbGdt;
    aValues[iReg].Table.Base  = pCtx->gdtr.pGdt;
    iReg++;

    /* Control registers. */
    aenmNames[iReg]     = WHvX64RegisterCr0;
    aValues[iReg].Reg64 = pCtx->cr0;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterCr2;
    aValues[iReg].Reg64 = pCtx->cr2;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterCr3;
    aValues[iReg].Reg64 = pCtx->cr3;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterCr4;
    aValues[iReg].Reg64 = pCtx->cr4;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterCr8;
    aValues[iReg].Reg64 = CPUMGetGuestCR8(pVCpu);
    iReg++;

    /* Debug registers. */
/** @todo fixme. Figure out what the hyper-v version of KVM_SET_GUEST_DEBUG would be. */
    aenmNames[iReg]     = WHvX64RegisterDr0;
    //aValues[iReg].Reg64 = CPUMGetHyperDR0(pVCpu);
    aValues[iReg].Reg64 = pCtx->dr[0];
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterDr1;
    //aValues[iReg].Reg64 = CPUMGetHyperDR1(pVCpu);
    aValues[iReg].Reg64 = pCtx->dr[1];
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterDr2;
    //aValues[iReg].Reg64 = CPUMGetHyperDR2(pVCpu);
    aValues[iReg].Reg64 = pCtx->dr[2];
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterDr3;
    //aValues[iReg].Reg64 = CPUMGetHyperDR3(pVCpu);
    aValues[iReg].Reg64 = pCtx->dr[3];
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterDr6;
    //aValues[iReg].Reg64 = CPUMGetHyperDR6(pVCpu);
    aValues[iReg].Reg64 = pCtx->dr[6];
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterDr7;
    //aValues[iReg].Reg64 = CPUMGetHyperDR7(pVCpu);
    aValues[iReg].Reg64 = pCtx->dr[7];
    iReg++;

    /* Vector state. */
    aenmNames[iReg]     = WHvX64RegisterXmm0;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[0].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[0].uXmm.s.Hi;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterXmm1;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[1].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[1].uXmm.s.Hi;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterXmm2;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[2].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[2].uXmm.s.Hi;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterXmm3;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[3].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[3].uXmm.s.Hi;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterXmm4;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[4].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[4].uXmm.s.Hi;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterXmm5;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[5].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[5].uXmm.s.Hi;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterXmm6;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[6].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[6].uXmm.s.Hi;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterXmm7;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[7].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[7].uXmm.s.Hi;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterXmm8;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[8].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[8].uXmm.s.Hi;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterXmm9;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[9].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[9].uXmm.s.Hi;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterXmm10;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[10].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[10].uXmm.s.Hi;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterXmm11;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[11].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[11].uXmm.s.Hi;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterXmm12;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[12].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[12].uXmm.s.Hi;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterXmm13;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[13].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[13].uXmm.s.Hi;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterXmm14;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[14].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[14].uXmm.s.Hi;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterXmm15;
    aValues[iReg].Reg128.Low64  = pCtx->pXStateR3->x87.aXMM[15].uXmm.s.Lo;
    aValues[iReg].Reg128.High64 = pCtx->pXStateR3->x87.aXMM[15].uXmm.s.Hi;
    iReg++;

    /* Floating point state. */
    aenmNames[iReg]     = WHvX64RegisterFpMmx0;
    aValues[iReg].Fp.AsUINT128.Low64  = pCtx->pXStateR3->x87.aRegs[0].au64[0];
    aValues[iReg].Fp.AsUINT128.High64 = pCtx->pXStateR3->x87.aRegs[0].au64[1];
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterFpMmx1;
    aValues[iReg].Fp.AsUINT128.Low64  = pCtx->pXStateR3->x87.aRegs[1].au64[0];
    aValues[iReg].Fp.AsUINT128.High64 = pCtx->pXStateR3->x87.aRegs[1].au64[1];
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterFpMmx2;
    aValues[iReg].Fp.AsUINT128.Low64  = pCtx->pXStateR3->x87.aRegs[2].au64[0];
    aValues[iReg].Fp.AsUINT128.High64 = pCtx->pXStateR3->x87.aRegs[2].au64[1];
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterFpMmx3;
    aValues[iReg].Fp.AsUINT128.Low64  = pCtx->pXStateR3->x87.aRegs[3].au64[0];
    aValues[iReg].Fp.AsUINT128.High64 = pCtx->pXStateR3->x87.aRegs[3].au64[1];
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterFpMmx4;
    aValues[iReg].Fp.AsUINT128.Low64  = pCtx->pXStateR3->x87.aRegs[4].au64[0];
    aValues[iReg].Fp.AsUINT128.High64 = pCtx->pXStateR3->x87.aRegs[4].au64[1];
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterFpMmx5;
    aValues[iReg].Fp.AsUINT128.Low64  = pCtx->pXStateR3->x87.aRegs[5].au64[0];
    aValues[iReg].Fp.AsUINT128.High64 = pCtx->pXStateR3->x87.aRegs[5].au64[1];
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterFpMmx6;
    aValues[iReg].Fp.AsUINT128.Low64  = pCtx->pXStateR3->x87.aRegs[6].au64[0];
    aValues[iReg].Fp.AsUINT128.High64 = pCtx->pXStateR3->x87.aRegs[6].au64[1];
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterFpMmx7;
    aValues[iReg].Fp.AsUINT128.Low64  = pCtx->pXStateR3->x87.aRegs[7].au64[0];
    aValues[iReg].Fp.AsUINT128.High64 = pCtx->pXStateR3->x87.aRegs[7].au64[1];
    iReg++;

    aenmNames[iReg]     = WHvX64RegisterFpControlStatus;
    aValues[iReg].FpControlStatus.FpControl = pCtx->pXStateR3->x87.FCW;
    aValues[iReg].FpControlStatus.FpStatus  = pCtx->pXStateR3->x87.FSW;
    aValues[iReg].FpControlStatus.FpTag     = pCtx->pXStateR3->x87.FTW;
    aValues[iReg].FpControlStatus.Reserved  = pCtx->pXStateR3->x87.FTW >> 8;
    aValues[iReg].FpControlStatus.LastFpOp  = pCtx->pXStateR3->x87.FOP;
    aValues[iReg].FpControlStatus.LastFpRip = (pCtx->pXStateR3->x87.FPUIP)
                                            | ((uint64_t)pCtx->pXStateR3->x87.CS << 32)
                                            | ((uint64_t)pCtx->pXStateR3->x87.Rsrvd1 << 48);
    iReg++;

    aenmNames[iReg]     = WHvX64RegisterXmmControlStatus;
    aValues[iReg].XmmControlStatus.LastFpRdp            = (pCtx->pXStateR3->x87.FPUDP)
                                                        | ((uint64_t)pCtx->pXStateR3->x87.DS << 32)
                                                        | ((uint64_t)pCtx->pXStateR3->x87.Rsrvd2 << 48);
    aValues[iReg].XmmControlStatus.XmmStatusControl     = pCtx->pXStateR3->x87.MXCSR;
    aValues[iReg].XmmControlStatus.XmmStatusControlMask = pCtx->pXStateR3->x87.MXCSR_MASK; /** @todo ??? (Isn't this an output field?) */
    iReg++;

    /* MSRs */
    // WHvX64RegisterTsc - don't touch
    aenmNames[iReg]     = WHvX64RegisterEfer;
    aValues[iReg].Reg64 = pCtx->msrEFER;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterKernelGsBase;
    aValues[iReg].Reg64 = pCtx->msrKERNELGSBASE;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterApicBase;
    aValues[iReg].Reg64 = APICGetBaseMsrNoCheck(pVCpu);
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterPat;
    aValues[iReg].Reg64 = pCtx->msrPAT;
    iReg++;
    /// @todo WHvX64RegisterSysenterCs
    /// @todo WHvX64RegisterSysenterEip
    /// @todo WHvX64RegisterSysenterEsp
    aenmNames[iReg]     = WHvX64RegisterStar;
    aValues[iReg].Reg64 = pCtx->msrSTAR;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterLstar;
    aValues[iReg].Reg64 = pCtx->msrLSTAR;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterCstar;
    aValues[iReg].Reg64 = pCtx->msrCSTAR;
    iReg++;
    aenmNames[iReg]     = WHvX64RegisterSfmask;
    aValues[iReg].Reg64 = pCtx->msrSFMASK;
    iReg++;

    /* event injection (always clear it). */
    aenmNames[iReg]     = WHvRegisterPendingInterruption;
    aValues[iReg].Reg64 = 0;
    iReg++;
    /// @todo WHvRegisterInterruptState
    /// @todo WHvRegisterPendingEvent0
    /// @todo WHvRegisterPendingEvent1

    /*
     * Set the registers.
     */
    Assert(iReg < RT_ELEMENTS(aValues));
    Assert(iReg < RT_ELEMENTS(aenmNames));
#  ifdef NEM_WIN_INTERCEPT_NT_IO_CTLS
    Log12(("Calling WHvSetVirtualProcessorRegisters(%p, %u, %p, %u, %p)\n",
           pVM->nem.s.hPartition, pVCpu->idCpu, aenmNames, iReg, aValues));
#  endif
    HRESULT hrc = WHvSetVirtualProcessorRegisters(pVM->nem.s.hPartition, pVCpu->idCpu, aenmNames, iReg, aValues);
    if (SUCCEEDED(hrc))
        return VINF_SUCCESS;
    AssertLogRelMsgFailed(("WHvSetVirtualProcessorRegisters(%p, %u,,%u,) -> %Rhrc (Last=%#x/%u)\n",
                           pVM->nem.s.hPartition, pVCpu->idCpu, iReg,
                           hrc, RTNtLastStatusValue(), RTNtLastErrorValue()));
    return VERR_INTERNAL_ERROR;
# endif /* !NEM_WIN_USE_HYPERCALLS_FOR_REGISTERS */
}


NEM_TMPL_STATIC int nemHCWinCopyStateFromHyperV(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, uint64_t fWhat)
{
# ifdef NEM_WIN_USE_HYPERCALLS_FOR_REGISTERS
    /* See NEMR0ImportState */
    NOREF(pCtx);
    int rc = VMMR3CallR0Emt(pVM, pVCpu, VMMR0_DO_NEM_IMPORT_STATE, fWhat, NULL);
    if (RT_SUCCESS(rc))
        return rc;
    if (rc == VERR_NEM_FLUSH_TLB)
        return PGMFlushTLB(pVCpu, pCtx->cr3, true /*fGlobal*/);
    if (rc == VERR_NEM_CHANGE_PGM_MODE)
        return PGMChangeMode(pVCpu, pCtx->cr0, pCtx->cr4, pCtx->msrEFER);
    AssertLogRelRCReturn(rc, rc);
    return rc;

# else
    WHV_REGISTER_NAME  aenmNames[128];

    /* GPRs */
    aenmNames[0]  = WHvX64RegisterRax;
    aenmNames[1]  = WHvX64RegisterRcx;
    aenmNames[2]  = WHvX64RegisterRdx;
    aenmNames[3]  = WHvX64RegisterRbx;
    aenmNames[4]  = WHvX64RegisterRsp;
    aenmNames[5]  = WHvX64RegisterRbp;
    aenmNames[6]  = WHvX64RegisterRsi;
    aenmNames[7]  = WHvX64RegisterRdi;
    aenmNames[8]  = WHvX64RegisterR8;
    aenmNames[9]  = WHvX64RegisterR9;
    aenmNames[10] = WHvX64RegisterR10;
    aenmNames[11] = WHvX64RegisterR11;
    aenmNames[12] = WHvX64RegisterR12;
    aenmNames[13] = WHvX64RegisterR13;
    aenmNames[14] = WHvX64RegisterR14;
    aenmNames[15] = WHvX64RegisterR15;

    /* RIP & Flags */
    aenmNames[16] = WHvX64RegisterRip;
    aenmNames[17] = WHvX64RegisterRflags;

    /* Segments */
    aenmNames[18] = WHvX64RegisterEs;
    aenmNames[19] = WHvX64RegisterCs;
    aenmNames[20] = WHvX64RegisterSs;
    aenmNames[21] = WHvX64RegisterDs;
    aenmNames[22] = WHvX64RegisterFs;
    aenmNames[23] = WHvX64RegisterGs;
    aenmNames[24] = WHvX64RegisterLdtr;
    aenmNames[25] = WHvX64RegisterTr;

    /* Descriptor tables. */
    aenmNames[26] = WHvX64RegisterIdtr;
    aenmNames[27] = WHvX64RegisterGdtr;

    /* Control registers. */
    aenmNames[28] = WHvX64RegisterCr0;
    aenmNames[29] = WHvX64RegisterCr2;
    aenmNames[30] = WHvX64RegisterCr3;
    aenmNames[31] = WHvX64RegisterCr4;
    aenmNames[32] = WHvX64RegisterCr8;

    /* Debug registers. */
    aenmNames[33] = WHvX64RegisterDr0;
    aenmNames[34] = WHvX64RegisterDr1;
    aenmNames[35] = WHvX64RegisterDr2;
    aenmNames[36] = WHvX64RegisterDr3;
    aenmNames[37] = WHvX64RegisterDr6;
    aenmNames[38] = WHvX64RegisterDr7;

    /* Vector state. */
    aenmNames[39] = WHvX64RegisterXmm0;
    aenmNames[40] = WHvX64RegisterXmm1;
    aenmNames[41] = WHvX64RegisterXmm2;
    aenmNames[42] = WHvX64RegisterXmm3;
    aenmNames[43] = WHvX64RegisterXmm4;
    aenmNames[44] = WHvX64RegisterXmm5;
    aenmNames[45] = WHvX64RegisterXmm6;
    aenmNames[46] = WHvX64RegisterXmm7;
    aenmNames[47] = WHvX64RegisterXmm8;
    aenmNames[48] = WHvX64RegisterXmm9;
    aenmNames[49] = WHvX64RegisterXmm10;
    aenmNames[50] = WHvX64RegisterXmm11;
    aenmNames[51] = WHvX64RegisterXmm12;
    aenmNames[52] = WHvX64RegisterXmm13;
    aenmNames[53] = WHvX64RegisterXmm14;
    aenmNames[54] = WHvX64RegisterXmm15;

    /* Floating point state. */
    aenmNames[55] = WHvX64RegisterFpMmx0;
    aenmNames[56] = WHvX64RegisterFpMmx1;
    aenmNames[57] = WHvX64RegisterFpMmx2;
    aenmNames[58] = WHvX64RegisterFpMmx3;
    aenmNames[59] = WHvX64RegisterFpMmx4;
    aenmNames[60] = WHvX64RegisterFpMmx5;
    aenmNames[61] = WHvX64RegisterFpMmx6;
    aenmNames[62] = WHvX64RegisterFpMmx7;
    aenmNames[63] = WHvX64RegisterFpControlStatus;
    aenmNames[64] = WHvX64RegisterXmmControlStatus;

    /* MSRs */
    // WHvX64RegisterTsc - don't touch
    aenmNames[65] = WHvX64RegisterEfer;
    aenmNames[66] = WHvX64RegisterKernelGsBase;
    aenmNames[67] = WHvX64RegisterApicBase;
    aenmNames[68] = WHvX64RegisterPat;
    aenmNames[69] = WHvX64RegisterSysenterCs;
    aenmNames[70] = WHvX64RegisterSysenterEip;
    aenmNames[71] = WHvX64RegisterSysenterEsp;
    aenmNames[72] = WHvX64RegisterStar;
    aenmNames[73] = WHvX64RegisterLstar;
    aenmNames[74] = WHvX64RegisterCstar;
    aenmNames[75] = WHvX64RegisterSfmask;

    /* event injection */
    aenmNames[76] = WHvRegisterPendingInterruption;
    aenmNames[77] = WHvRegisterInterruptState;
    aenmNames[78] = WHvRegisterInterruptState;
    aenmNames[79] = WHvRegisterPendingEvent0;
    aenmNames[80] = WHvRegisterPendingEvent1;
    unsigned const cRegs = 81;

    /*
     * Get the registers.
     */
    WHV_REGISTER_VALUE aValues[cRegs];
    RT_ZERO(aValues);
    Assert(RT_ELEMENTS(aValues) >= cRegs);
    Assert(RT_ELEMENTS(aenmNames) >= cRegs);
#  ifdef NEM_WIN_INTERCEPT_NT_IO_CTLS
    Log12(("Calling WHvGetVirtualProcessorRegisters(%p, %u, %p, %u, %p)\n",
          pVM->nem.s.hPartition, pVCpu->idCpu, aenmNames, cRegs, aValues));
#  endif
    HRESULT hrc = WHvGetVirtualProcessorRegisters(pVM->nem.s.hPartition, pVCpu->idCpu, aenmNames, cRegs, aValues);
    if (SUCCEEDED(hrc))
    {
        /* GPRs */
        Assert(aenmNames[0]  == WHvX64RegisterRax);
        Assert(aenmNames[15] == WHvX64RegisterR15);
        pCtx->rax = aValues[0].Reg64;
        pCtx->rcx = aValues[1].Reg64;
        pCtx->rdx = aValues[2].Reg64;
        pCtx->rbx = aValues[3].Reg64;
        pCtx->rsp = aValues[4].Reg64;
        pCtx->rbp = aValues[5].Reg64;
        pCtx->rsi = aValues[6].Reg64;
        pCtx->rdi = aValues[7].Reg64;
        pCtx->r8  = aValues[8].Reg64;
        pCtx->r9  = aValues[9].Reg64;
        pCtx->r10 = aValues[10].Reg64;
        pCtx->r11 = aValues[11].Reg64;
        pCtx->r12 = aValues[12].Reg64;
        pCtx->r13 = aValues[13].Reg64;
        pCtx->r14 = aValues[14].Reg64;
        pCtx->r15 = aValues[15].Reg64;

        /* RIP & Flags */
        Assert(aenmNames[16] == WHvX64RegisterRip);
        pCtx->rip      = aValues[16].Reg64;
        pCtx->rflags.u = aValues[17].Reg64;

        /* Segments */
#  define COPY_BACK_SEG(a_idx, a_enmName, a_SReg) \
            do { \
                Assert(aenmNames[a_idx] == a_enmName); \
                NEM_WIN_COPY_BACK_SEG(a_SReg, aValues[a_idx]); \
            } while (0)
        COPY_BACK_SEG(18, WHvX64RegisterEs,   pCtx->es);
        COPY_BACK_SEG(19, WHvX64RegisterCs,   pCtx->cs);
        COPY_BACK_SEG(20, WHvX64RegisterSs,   pCtx->ss);
        COPY_BACK_SEG(21, WHvX64RegisterDs,   pCtx->ds);
        COPY_BACK_SEG(22, WHvX64RegisterFs,   pCtx->fs);
        COPY_BACK_SEG(23, WHvX64RegisterGs,   pCtx->gs);
        COPY_BACK_SEG(24, WHvX64RegisterLdtr, pCtx->ldtr);
        /* AMD-V likes loading TR with in AVAIL state, whereas intel insists on BUSY.  So,
           avoid to trigger sanity assertions around the code, always fix this. */
        COPY_BACK_SEG(25, WHvX64RegisterTr,   pCtx->tr);
        switch (pCtx->tr.Attr.n.u4Type)
        {
            case X86_SEL_TYPE_SYS_386_TSS_BUSY:
            case X86_SEL_TYPE_SYS_286_TSS_BUSY:
                break;
            case X86_SEL_TYPE_SYS_386_TSS_AVAIL:
                pCtx->tr.Attr.n.u4Type = X86_SEL_TYPE_SYS_386_TSS_BUSY;
                break;
            case X86_SEL_TYPE_SYS_286_TSS_AVAIL:
                pCtx->tr.Attr.n.u4Type = X86_SEL_TYPE_SYS_286_TSS_BUSY;
                break;
        }

        /* Descriptor tables. */
        Assert(aenmNames[26] == WHvX64RegisterIdtr);
        pCtx->idtr.cbIdt = aValues[26].Table.Limit;
        pCtx->idtr.pIdt  = aValues[26].Table.Base;
        Assert(aenmNames[27] == WHvX64RegisterGdtr);
        pCtx->gdtr.cbGdt = aValues[27].Table.Limit;
        pCtx->gdtr.pGdt  = aValues[27].Table.Base;

        /* Control registers. */
        Assert(aenmNames[28] == WHvX64RegisterCr0);
        bool fMaybeChangedMode = false;
        bool fFlushTlb         = false;
        bool fFlushGlobalTlb   = false;
        if (pCtx->cr0 != aValues[28].Reg64)
        {
            CPUMSetGuestCR0(pVCpu, aValues[28].Reg64);
            fMaybeChangedMode = true;
            fFlushTlb = fFlushGlobalTlb = true; /// @todo fix this
        }
        Assert(aenmNames[29] == WHvX64RegisterCr2);
        pCtx->cr2 = aValues[29].Reg64;
        if (pCtx->cr3 != aValues[30].Reg64)
        {
            CPUMSetGuestCR3(pVCpu, aValues[30].Reg64);
            fFlushTlb = true;
        }
        if (pCtx->cr4 != aValues[31].Reg64)
        {
            CPUMSetGuestCR4(pVCpu, aValues[31].Reg64);
            fMaybeChangedMode = true;
            fFlushTlb = fFlushGlobalTlb = true; /// @todo fix this
        }
        APICSetTpr(pVCpu, (uint8_t)aValues[32].Reg64 << 4);

        /* Debug registers. */
        Assert(aenmNames[33] == WHvX64RegisterDr0);
    /** @todo fixme */
        if (pCtx->dr[0] != aValues[33].Reg64)
            CPUMSetGuestDR0(pVCpu, aValues[33].Reg64);
        if (pCtx->dr[1] != aValues[34].Reg64)
            CPUMSetGuestDR1(pVCpu, aValues[34].Reg64);
        if (pCtx->dr[2] != aValues[35].Reg64)
            CPUMSetGuestDR2(pVCpu, aValues[35].Reg64);
        if (pCtx->dr[3] != aValues[36].Reg64)
            CPUMSetGuestDR3(pVCpu, aValues[36].Reg64);
        Assert(aenmNames[37] == WHvX64RegisterDr6);
        Assert(aenmNames[38] == WHvX64RegisterDr7);
        if (pCtx->dr[6] != aValues[37].Reg64)
            CPUMSetGuestDR6(pVCpu, aValues[37].Reg64);
        if (pCtx->dr[7] != aValues[38].Reg64)
            CPUMSetGuestDR6(pVCpu, aValues[38].Reg64);

        /* Vector state. */
        Assert(aenmNames[39] == WHvX64RegisterXmm0);
        Assert(aenmNames[54] == WHvX64RegisterXmm15);
        pCtx->pXStateR3->x87.aXMM[0].uXmm.s.Lo  = aValues[39].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[0].uXmm.s.Hi  = aValues[39].Reg128.High64;
        pCtx->pXStateR3->x87.aXMM[1].uXmm.s.Lo  = aValues[40].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[1].uXmm.s.Hi  = aValues[40].Reg128.High64;
        pCtx->pXStateR3->x87.aXMM[2].uXmm.s.Lo  = aValues[41].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[2].uXmm.s.Hi  = aValues[41].Reg128.High64;
        pCtx->pXStateR3->x87.aXMM[3].uXmm.s.Lo  = aValues[42].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[3].uXmm.s.Hi  = aValues[42].Reg128.High64;
        pCtx->pXStateR3->x87.aXMM[4].uXmm.s.Lo  = aValues[43].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[4].uXmm.s.Hi  = aValues[43].Reg128.High64;
        pCtx->pXStateR3->x87.aXMM[5].uXmm.s.Lo  = aValues[44].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[5].uXmm.s.Hi  = aValues[44].Reg128.High64;
        pCtx->pXStateR3->x87.aXMM[6].uXmm.s.Lo  = aValues[45].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[6].uXmm.s.Hi  = aValues[45].Reg128.High64;
        pCtx->pXStateR3->x87.aXMM[7].uXmm.s.Lo  = aValues[46].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[7].uXmm.s.Hi  = aValues[46].Reg128.High64;
        pCtx->pXStateR3->x87.aXMM[8].uXmm.s.Lo  = aValues[47].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[8].uXmm.s.Hi  = aValues[47].Reg128.High64;
        pCtx->pXStateR3->x87.aXMM[9].uXmm.s.Lo  = aValues[48].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[9].uXmm.s.Hi  = aValues[48].Reg128.High64;
        pCtx->pXStateR3->x87.aXMM[10].uXmm.s.Lo = aValues[49].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[10].uXmm.s.Hi = aValues[49].Reg128.High64;
        pCtx->pXStateR3->x87.aXMM[11].uXmm.s.Lo = aValues[50].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[11].uXmm.s.Hi = aValues[50].Reg128.High64;
        pCtx->pXStateR3->x87.aXMM[12].uXmm.s.Lo = aValues[51].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[12].uXmm.s.Hi = aValues[51].Reg128.High64;
        pCtx->pXStateR3->x87.aXMM[13].uXmm.s.Lo = aValues[52].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[13].uXmm.s.Hi = aValues[52].Reg128.High64;
        pCtx->pXStateR3->x87.aXMM[14].uXmm.s.Lo = aValues[53].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[14].uXmm.s.Hi = aValues[53].Reg128.High64;
        pCtx->pXStateR3->x87.aXMM[15].uXmm.s.Lo = aValues[54].Reg128.Low64;
        pCtx->pXStateR3->x87.aXMM[15].uXmm.s.Hi = aValues[54].Reg128.High64;

        /* Floating point state. */
        Assert(aenmNames[55] == WHvX64RegisterFpMmx0);
        Assert(aenmNames[62] == WHvX64RegisterFpMmx7);
        pCtx->pXStateR3->x87.aRegs[0].au64[0] = aValues[55].Fp.AsUINT128.Low64;
        pCtx->pXStateR3->x87.aRegs[0].au64[1] = aValues[55].Fp.AsUINT128.High64;
        pCtx->pXStateR3->x87.aRegs[1].au64[0] = aValues[56].Fp.AsUINT128.Low64;
        pCtx->pXStateR3->x87.aRegs[1].au64[1] = aValues[56].Fp.AsUINT128.High64;
        pCtx->pXStateR3->x87.aRegs[2].au64[0] = aValues[57].Fp.AsUINT128.Low64;
        pCtx->pXStateR3->x87.aRegs[2].au64[1] = aValues[57].Fp.AsUINT128.High64;
        pCtx->pXStateR3->x87.aRegs[3].au64[0] = aValues[58].Fp.AsUINT128.Low64;
        pCtx->pXStateR3->x87.aRegs[3].au64[1] = aValues[58].Fp.AsUINT128.High64;
        pCtx->pXStateR3->x87.aRegs[4].au64[0] = aValues[59].Fp.AsUINT128.Low64;
        pCtx->pXStateR3->x87.aRegs[4].au64[1] = aValues[59].Fp.AsUINT128.High64;
        pCtx->pXStateR3->x87.aRegs[5].au64[0] = aValues[60].Fp.AsUINT128.Low64;
        pCtx->pXStateR3->x87.aRegs[5].au64[1] = aValues[60].Fp.AsUINT128.High64;
        pCtx->pXStateR3->x87.aRegs[6].au64[0] = aValues[61].Fp.AsUINT128.Low64;
        pCtx->pXStateR3->x87.aRegs[6].au64[1] = aValues[61].Fp.AsUINT128.High64;
        pCtx->pXStateR3->x87.aRegs[7].au64[0] = aValues[62].Fp.AsUINT128.Low64;
        pCtx->pXStateR3->x87.aRegs[7].au64[1] = aValues[62].Fp.AsUINT128.High64;

        Assert(aenmNames[63] == WHvX64RegisterFpControlStatus);
        pCtx->pXStateR3->x87.FCW        = aValues[63].FpControlStatus.FpControl;
        pCtx->pXStateR3->x87.FSW        = aValues[63].FpControlStatus.FpStatus;
        pCtx->pXStateR3->x87.FTW        = aValues[63].FpControlStatus.FpTag
                                        /*| (aValues[63].FpControlStatus.Reserved << 8)*/;
        pCtx->pXStateR3->x87.FOP        = aValues[63].FpControlStatus.LastFpOp;
        pCtx->pXStateR3->x87.FPUIP      = (uint32_t)aValues[63].FpControlStatus.LastFpRip;
        pCtx->pXStateR3->x87.CS         = (uint16_t)(aValues[63].FpControlStatus.LastFpRip >> 32);
        pCtx->pXStateR3->x87.Rsrvd1     = (uint16_t)(aValues[63].FpControlStatus.LastFpRip >> 48);

        Assert(aenmNames[64] == WHvX64RegisterXmmControlStatus);
        pCtx->pXStateR3->x87.FPUDP      = (uint32_t)aValues[64].XmmControlStatus.LastFpRdp;
        pCtx->pXStateR3->x87.DS         = (uint16_t)(aValues[64].XmmControlStatus.LastFpRdp >> 32);
        pCtx->pXStateR3->x87.Rsrvd2     = (uint16_t)(aValues[64].XmmControlStatus.LastFpRdp >> 48);
        pCtx->pXStateR3->x87.MXCSR      = aValues[64].XmmControlStatus.XmmStatusControl;
        pCtx->pXStateR3->x87.MXCSR_MASK = aValues[64].XmmControlStatus.XmmStatusControlMask; /** @todo ??? (Isn't this an output field?) */

        /* MSRs */
        // WHvX64RegisterTsc - don't touch
        Assert(aenmNames[65] == WHvX64RegisterEfer);
        if (aValues[65].Reg64 != pCtx->msrEFER)
        {
            pCtx->msrEFER = aValues[65].Reg64;
            fMaybeChangedMode = true;
        }

        Assert(aenmNames[66] == WHvX64RegisterKernelGsBase);
        pCtx->msrKERNELGSBASE = aValues[66].Reg64;

        Assert(aenmNames[67] == WHvX64RegisterApicBase);
        if (aValues[67].Reg64 != APICGetBaseMsrNoCheck(pVCpu))
        {
            VBOXSTRICTRC rc2 = APICSetBaseMsr(pVCpu, aValues[67].Reg64);
            Assert(rc2 == VINF_SUCCESS); NOREF(rc2);
        }

        Assert(aenmNames[68] == WHvX64RegisterPat);
        pCtx->msrPAT    = aValues[68].Reg64;
        /// @todo WHvX64RegisterSysenterCs
        /// @todo WHvX64RegisterSysenterEip
        /// @todo WHvX64RegisterSysenterEsp
        Assert(aenmNames[72] == WHvX64RegisterStar);
        pCtx->msrSTAR   = aValues[72].Reg64;
        Assert(aenmNames[73] == WHvX64RegisterLstar);
        pCtx->msrLSTAR  = aValues[73].Reg64;
        Assert(aenmNames[74] == WHvX64RegisterCstar);
        pCtx->msrCSTAR  = aValues[74].Reg64;
        Assert(aenmNames[75] == WHvX64RegisterSfmask);
        pCtx->msrSFMASK = aValues[75].Reg64;

        /// @todo WHvRegisterPendingInterruption
        Assert(aenmNames[76] == WHvRegisterPendingInterruption);
        WHV_X64_PENDING_INTERRUPTION_REGISTER const * pPendingInt = (WHV_X64_PENDING_INTERRUPTION_REGISTER const *)&aValues[76];
        if (pPendingInt->InterruptionPending)
        {
            Log7(("PendingInterruption: type=%u vector=%#x errcd=%RTbool/%#x instr-len=%u nested=%u\n",
                  pPendingInt->InterruptionType, pPendingInt->InterruptionVector, pPendingInt->DeliverErrorCode,
                  pPendingInt->ErrorCode, pPendingInt->InstructionLength, pPendingInt->NestedEvent));
            AssertMsg((pPendingInt->AsUINT64 & UINT64_C(0xfc00)) == 0, ("%#RX64\n", pPendingInt->AsUINT64));
        }

        /// @todo WHvRegisterInterruptState
        /// @todo WHvRegisterPendingEvent0
        /// @todo WHvRegisterPendingEvent1

        pCtx->fExtrn = 0;

        if (fMaybeChangedMode)
        {
            int rc = PGMChangeMode(pVCpu, pCtx->cr0, pCtx->cr4, pCtx->msrEFER);
            AssertRC(rc);
        }
        if (fFlushTlb)
        {
            int rc = PGMFlushTLB(pVCpu, pCtx->cr3, fFlushGlobalTlb);
            AssertRC(rc);
        }

        return VINF_SUCCESS;
    }

    AssertLogRelMsgFailed(("WHvGetVirtualProcessorRegisters(%p, %u,,%u,) -> %Rhrc (Last=%#x/%u)\n",
                           pVM->nem.s.hPartition, pVCpu->idCpu, cRegs,
                           hrc, RTNtLastStatusValue(), RTNtLastErrorValue()));
    return VERR_INTERNAL_ERROR;
# endif /* !NEM_WIN_USE_HYPERCALLS_FOR_REGISTERS */
}

#endif /* !IN_RING0 */


#ifdef LOG_ENABLED
/**
 * Get the virtual processor running status.
 */
DECLINLINE(VID_PROCESSOR_STATUS) nemHCWinCpuGetRunningStatus(PVMCPU pVCpu)
{
# ifdef IN_RING0
    NOREF(pVCpu);
    return VidProcessorStatusUndefined;
# else
    RTERRVARS Saved;
    RTErrVarsSave(&Saved);

    /*
     * This API is disabled in release builds, it seems.  On build 17101 it requires
     * the following patch to be enabled (windbg): eb vid+12180 0f 84 98 00 00 00
     */
    VID_PROCESSOR_STATUS enmCpuStatus = VidProcessorStatusUndefined;
    NTSTATUS rcNt = g_pfnVidGetVirtualProcessorRunningStatus(pVCpu->pVMR3->nem.s.hPartitionDevice, pVCpu->idCpu, &enmCpuStatus);
    AssertRC(rcNt);

    RTErrVarsRestore(&Saved);
    return enmCpuStatus;
# endif
}
#endif


#ifdef NEM_WIN_USE_OUR_OWN_RUN_API
# ifdef IN_RING3 /* hopefully not needed in ring-0, as we'd need KTHREADs and KeAlertThread. */
/**
 * Our own WHvCancelRunVirtualProcessor that can later be moved to ring-0.
 *
 * This is an experiment only.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context virtual CPU structure of the
 *                          calling EMT.
 */
NEM_TMPL_STATIC int nemHCWinCancelRunVirtualProcessor(PVM pVM, PVMCPU pVCpu)
{
    /*
     * Work the state.
     *
     * From the looks of things, we should let the EMT call VidStopVirtualProcessor.
     * So, we just need to modify the state and kick the EMT if it's waiting on
     * messages.  For the latter we use QueueUserAPC / KeAlterThread.
     */
    for (;;)
    {
        VMCPUSTATE enmState = VMCPU_GET_STATE(pVCpu);
        switch (enmState)
        {
            case VMCPUSTATE_STARTED_EXEC_NEM:
                if (VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC_NEM_CANCELED, VMCPUSTATE_STARTED_EXEC_NEM))
                {
                    Log8(("nemHCWinCancelRunVirtualProcessor: Switched %u to canceled state\n", pVCpu->idCpu));
                    STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatCancelChangedState);
                    return VINF_SUCCESS;
                }
                break;

            case VMCPUSTATE_STARTED_EXEC_NEM_WAIT:
                if (VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC_NEM_CANCELED, VMCPUSTATE_STARTED_EXEC_NEM_WAIT))
                {
#  ifdef IN_RING0
                    NTSTATUS rcNt = KeAlertThread(??);
#  else
                    NTSTATUS rcNt = NtAlertThread(pVCpu->nem.s.hNativeThreadHandle);
#  endif
                    Log8(("nemHCWinCancelRunVirtualProcessor: Alerted %u: %#x\n", pVCpu->idCpu, rcNt));
                    Assert(rcNt == STATUS_SUCCESS);
                    if (NT_SUCCESS(rcNt))
                    {
                        STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatCancelAlertedThread);
                        return VINF_SUCCESS;
                    }
                    AssertLogRelMsgFailedReturn(("NtAlertThread failed: %#x\n", rcNt), RTErrConvertFromNtStatus(rcNt));
                }
                break;

            default:
                return VINF_SUCCESS;
        }

        ASMNopPause();
        RT_NOREF(pVM);
    }
}
# endif /* IN_RING3 */
#endif /* NEM_WIN_USE_OUR_OWN_RUN_API */


#ifdef LOG_ENABLED
/**
 * Logs the current CPU state.
 */
NEM_TMPL_STATIC void nemHCWinLogState(PVM pVM, PVMCPU pVCpu)
{
    if (LogIs3Enabled())
    {
# ifdef IN_RING3
        char szRegs[4096];
        DBGFR3RegPrintf(pVM->pUVM, pVCpu->idCpu, &szRegs[0], sizeof(szRegs),
                        "rax=%016VR{rax} rbx=%016VR{rbx} rcx=%016VR{rcx} rdx=%016VR{rdx}\n"
                        "rsi=%016VR{rsi} rdi=%016VR{rdi} r8 =%016VR{r8} r9 =%016VR{r9}\n"
                        "r10=%016VR{r10} r11=%016VR{r11} r12=%016VR{r12} r13=%016VR{r13}\n"
                        "r14=%016VR{r14} r15=%016VR{r15} %VRF{rflags}\n"
                        "rip=%016VR{rip} rsp=%016VR{rsp} rbp=%016VR{rbp}\n"
                        "cs={%04VR{cs} base=%016VR{cs_base} limit=%08VR{cs_lim} flags=%04VR{cs_attr}} cr0=%016VR{cr0}\n"
                        "ds={%04VR{ds} base=%016VR{ds_base} limit=%08VR{ds_lim} flags=%04VR{ds_attr}} cr2=%016VR{cr2}\n"
                        "es={%04VR{es} base=%016VR{es_base} limit=%08VR{es_lim} flags=%04VR{es_attr}} cr3=%016VR{cr3}\n"
                        "fs={%04VR{fs} base=%016VR{fs_base} limit=%08VR{fs_lim} flags=%04VR{fs_attr}} cr4=%016VR{cr4}\n"
                        "gs={%04VR{gs} base=%016VR{gs_base} limit=%08VR{gs_lim} flags=%04VR{gs_attr}} cr8=%016VR{cr8}\n"
                        "ss={%04VR{ss} base=%016VR{ss_base} limit=%08VR{ss_lim} flags=%04VR{ss_attr}}\n"
                        "dr0=%016VR{dr0} dr1=%016VR{dr1} dr2=%016VR{dr2} dr3=%016VR{dr3}\n"
                        "dr6=%016VR{dr6} dr7=%016VR{dr7}\n"
                        "gdtr=%016VR{gdtr_base}:%04VR{gdtr_lim}  idtr=%016VR{idtr_base}:%04VR{idtr_lim}  rflags=%08VR{rflags}\n"
                        "ldtr={%04VR{ldtr} base=%016VR{ldtr_base} limit=%08VR{ldtr_lim} flags=%08VR{ldtr_attr}}\n"
                        "tr  ={%04VR{tr} base=%016VR{tr_base} limit=%08VR{tr_lim} flags=%08VR{tr_attr}}\n"
                        "    sysenter={cs=%04VR{sysenter_cs} eip=%08VR{sysenter_eip} esp=%08VR{sysenter_esp}}\n"
                        "        efer=%016VR{efer}\n"
                        "         pat=%016VR{pat}\n"
                        "     sf_mask=%016VR{sf_mask}\n"
                        "krnl_gs_base=%016VR{krnl_gs_base}\n"
                        "       lstar=%016VR{lstar}\n"
                        "        star=%016VR{star} cstar=%016VR{cstar}\n"
                        "fcw=%04VR{fcw} fsw=%04VR{fsw} ftw=%04VR{ftw} mxcsr=%04VR{mxcsr} mxcsr_mask=%04VR{mxcsr_mask}\n"
                        );

        char szInstr[256];
        DBGFR3DisasInstrEx(pVM->pUVM, pVCpu->idCpu, 0, 0,
                           DBGF_DISAS_FLAGS_CURRENT_GUEST | DBGF_DISAS_FLAGS_DEFAULT_MODE,
                           szInstr, sizeof(szInstr), NULL);
        Log3(("%s%s\n", szRegs, szInstr));
# else
        /** @todo stat logging in ring-0 */
        RT_NOREF(pVM, pVCpu);
# endif
    }
}
#endif /* LOG_ENABLED */


/**
 * Advances the guest RIP and clear EFLAGS.RF.
 *
 * This may clear VMCPU_FF_INHIBIT_INTERRUPTS.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pCtx            The CPU context to update.
 * @param   pExitCtx        The exit context.
 */
DECLINLINE(void) nemHCWinAdvanceGuestRipAndClearRF(PVMCPU pVCpu, PCPUMCTX pCtx, HV_X64_INTERCEPT_MESSAGE_HEADER const *pMsgHdr)
{
    Assert(!(pCtx->fExtrn & (CPUMCTX_EXTRN_RIP | CPUMCTX_EXTRN_RFLAGS)));

    /* Advance the RIP. */
    Assert(pMsgHdr->InstructionLength > 0 && pMsgHdr->InstructionLength < 16);
    pCtx->rip += pMsgHdr->InstructionLength;
    pCtx->rflags.Bits.u1RF = 0;

    /* Update interrupt inhibition. */
    if (!VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS))
    { /* likely */ }
    else if (pCtx->rip != EMGetInhibitInterruptsPC(pVCpu))
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);
}


NEM_TMPL_STATIC VBOXSTRICTRC
nemHCWinHandleHalt(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    NOREF(pVM); NOREF(pVCpu); NOREF(pCtx);
    LogFlow(("nemHCWinHandleHalt\n"));
    return VINF_EM_HALT;
}


NEM_TMPL_STATIC DECLCALLBACK(int)
nemHCWinUnmapOnePageCallback(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhys, uint8_t *pu2NemState, void *pvUser)
{
    RT_NOREF_PV(pvUser);
#ifdef NEM_WIN_USE_HYPERCALLS_FOR_PAGES
    int rc = nemHCWinHypercallUnmapPage(pVM, pVCpu, GCPhys);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
#else
    RT_NOREF_PV(pVCpu);
    HRESULT hrc = WHvUnmapGpaRange(pVM->nem.s.hPartition, GCPhys, X86_PAGE_SIZE);
    if (SUCCEEDED(hrc))
#endif
    {
        Log5(("NEM GPA unmap all: %RGp (cMappedPages=%u)\n", GCPhys, pVM->nem.s.cMappedPages - 1));
        *pu2NemState = NEM_WIN_PAGE_STATE_UNMAPPED;
    }
    else
    {
#ifdef NEM_WIN_USE_HYPERCALLS_FOR_PAGES
        LogRel(("nemR3WinUnmapOnePageCallback: GCPhys=%RGp rc=%Rrc\n", GCPhys, rc));
#else
        LogRel(("nemR3WinUnmapOnePageCallback: GCPhys=%RGp %s hrc=%Rhrc (%#x) Last=%#x/%u (cMappedPages=%u)\n",
                GCPhys, g_apszPageStates[*pu2NemState], hrc, hrc, RTNtLastStatusValue(),
                RTNtLastErrorValue(), pVM->nem.s.cMappedPages));
#endif
        *pu2NemState = NEM_WIN_PAGE_STATE_NOT_SET;
    }
    if (pVM->nem.s.cMappedPages > 0)
        ASMAtomicDecU32(&pVM->nem.s.cMappedPages);
    return VINF_SUCCESS;
}


/**
 * State to pass between nemHCWinHandleMemoryAccess / nemR3WinWHvHandleMemoryAccess
 * and nemHCWinHandleMemoryAccessPageCheckerCallback.
 */
typedef struct NEMHCWINHMACPCCSTATE
{
    /** Input: Write access. */
    bool    fWriteAccess;
    /** Output: Set if we did something. */
    bool    fDidSomething;
    /** Output: Set it we should resume. */
    bool    fCanResume;
} NEMHCWINHMACPCCSTATE;

/**
 * @callback_method_impl{FNPGMPHYSNEMCHECKPAGE,
 *      Worker for nemR3WinHandleMemoryAccess; pvUser points to a
 *      NEMHCWINHMACPCCSTATE structure. }
 */
NEM_TMPL_STATIC DECLCALLBACK(int)
nemHCWinHandleMemoryAccessPageCheckerCallback(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhys, PPGMPHYSNEMPAGEINFO pInfo, void *pvUser)
{
    NEMHCWINHMACPCCSTATE *pState = (NEMHCWINHMACPCCSTATE *)pvUser;
    pState->fDidSomething = false;
    pState->fCanResume    = false;

    /* If A20 is disabled, we may need to make another query on the masked
       page to get the correct protection information. */
    uint8_t  u2State = pInfo->u2NemState;
    RTGCPHYS GCPhysSrc;
    if (   pVM->nem.s.fA20Enabled
        || !NEM_WIN_IS_SUBJECT_TO_A20(GCPhys))
        GCPhysSrc = GCPhys;
    else
    {
        GCPhysSrc = GCPhys & ~(RTGCPHYS)RT_BIT_32(20);
        PGMPHYSNEMPAGEINFO Info2;
        int rc = PGMPhysNemPageInfoChecker(pVM, pVCpu, GCPhysSrc, pState->fWriteAccess, &Info2, NULL, NULL);
        AssertRCReturn(rc, rc);

        *pInfo = Info2;
        pInfo->u2NemState = u2State;
    }

    /*
     * Consolidate current page state with actual page protection and access type.
     * We don't really consider downgrades here, as they shouldn't happen.
     */
#ifndef NEM_WIN_USE_HYPERCALLS_FOR_PAGES
    /** @todo Someone at microsoft please explain:
     * I'm not sure WTF was going on, but I ended up in a loop if I remapped a
     * readonly page as writable (unmap, then map again).  Specifically, this was an
     * issue with the big VRAM mapping at 0xe0000000 when booing DSL 4.4.1.  So, in
     * a hope to work around that we no longer pre-map anything, just unmap stuff
     * and do it lazily here.  And here we will first unmap, restart, and then remap
     * with new protection or backing.
     */
#endif
    int rc;
    switch (u2State)
    {
        case NEM_WIN_PAGE_STATE_UNMAPPED:
        case NEM_WIN_PAGE_STATE_NOT_SET:
            if (pInfo->fNemProt == NEM_PAGE_PROT_NONE)
            {
                Log4(("nemHCWinHandleMemoryAccessPageCheckerCallback: %RGp - #1\n", GCPhys));
                return VINF_SUCCESS;
            }

            /* Don't bother remapping it if it's a write request to a non-writable page. */
            if (   pState->fWriteAccess
                && !(pInfo->fNemProt & NEM_PAGE_PROT_WRITE))
            {
                Log4(("nemHCWinHandleMemoryAccessPageCheckerCallback: %RGp - #1w\n", GCPhys));
                return VINF_SUCCESS;
            }

            /* Map the page. */
            rc = nemHCNativeSetPhysPage(pVM,
                                        pVCpu,
                                        GCPhysSrc & ~(RTGCPHYS)X86_PAGE_OFFSET_MASK,
                                        GCPhys & ~(RTGCPHYS)X86_PAGE_OFFSET_MASK,
                                        pInfo->fNemProt,
                                        &u2State,
                                        true /*fBackingState*/);
            pInfo->u2NemState = u2State;
            Log4(("nemHCWinHandleMemoryAccessPageCheckerCallback: %RGp - synced => %s + %Rrc\n",
                  GCPhys, g_apszPageStates[u2State], rc));
            pState->fDidSomething = true;
            pState->fCanResume    = true;
            return rc;

        case NEM_WIN_PAGE_STATE_READABLE:
            if (   !(pInfo->fNemProt & NEM_PAGE_PROT_WRITE)
                && (pInfo->fNemProt & (NEM_PAGE_PROT_READ | NEM_PAGE_PROT_EXECUTE)))
            {
                Log4(("nemHCWinHandleMemoryAccessPageCheckerCallback: %RGp - #2\n", GCPhys));
                return VINF_SUCCESS;
            }

#ifdef NEM_WIN_USE_HYPERCALLS_FOR_PAGES
            /* Upgrade page to writable. */
/** @todo test this*/
            if (   (pInfo->fNemProt & NEM_PAGE_PROT_WRITE)
                && pState->fWriteAccess)
            {
                rc = nemHCWinHypercallMapPage(pVM, pVCpu, GCPhysSrc, GCPhys,
                                              HV_MAP_GPA_READABLE   | HV_MAP_GPA_WRITABLE
                                              | HV_MAP_GPA_EXECUTABLE | HV_MAP_GPA_EXECUTABLE_AGAIN);
                AssertRC(rc);
                if (RT_SUCCESS(rc))
                {
                    pInfo->u2NemState = NEM_WIN_PAGE_STATE_WRITABLE;
                    pState->fDidSomething = true;
                    pState->fCanResume    = true;
                    Log5(("NEM GPA write-upgrade/exit: %RGp (was %s, cMappedPages=%u)\n",
                          GCPhys, g_apszPageStates[u2State], pVM->nem.s.cMappedPages));
                }
            }
            else
            {
                /* Need to emulate the acces. */
                AssertBreak(pInfo->fNemProt != NEM_PAGE_PROT_NONE); /* There should be no downgrades. */
                rc = VINF_SUCCESS;
            }
            return rc;
#else
            break;
#endif

        case NEM_WIN_PAGE_STATE_WRITABLE:
            if (pInfo->fNemProt & NEM_PAGE_PROT_WRITE)
            {
                Log4(("nemHCWinHandleMemoryAccessPageCheckerCallback: %RGp - #3\n", GCPhys));
                return VINF_SUCCESS;
            }
#ifdef NEM_WIN_USE_HYPERCALLS_FOR_PAGES
            AssertFailed(); /* There should be no downgrades. */
#endif
            break;

        default:
            AssertLogRelMsgFailedReturn(("u2State=%#x\n", u2State), VERR_INTERNAL_ERROR_3);
    }

    /*
     * Unmap and restart the instruction.
     * If this fails, which it does every so often, just unmap everything for now.
     */
#ifdef NEM_WIN_USE_HYPERCALLS_FOR_PAGES
    rc = nemHCWinHypercallUnmapPage(pVM, pVCpu, GCPhys);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
#else
    /** @todo figure out whether we mess up the state or if it's WHv.   */
    HRESULT hrc = WHvUnmapGpaRange(pVM->nem.s.hPartition, GCPhys, X86_PAGE_SIZE);
    if (SUCCEEDED(hrc))
#endif
    {
        pState->fDidSomething = true;
        pState->fCanResume    = true;
        pInfo->u2NemState = NEM_WIN_PAGE_STATE_UNMAPPED;
        uint32_t cMappedPages = ASMAtomicDecU32(&pVM->nem.s.cMappedPages); NOREF(cMappedPages);
        Log5(("NEM GPA unmapped/exit: %RGp (was %s, cMappedPages=%u)\n", GCPhys, g_apszPageStates[u2State], cMappedPages));
        return VINF_SUCCESS;
    }
#ifdef NEM_WIN_USE_HYPERCALLS_FOR_PAGES
    LogRel(("nemHCWinHandleMemoryAccessPageCheckerCallback/unmap: GCPhysDst=%RGp rc=%Rrc\n", GCPhys, rc));
    return rc;
#else
    LogRel(("nemHCWinHandleMemoryAccessPageCheckerCallback/unmap: GCPhysDst=%RGp %s hrc=%Rhrc (%#x) Last=%#x/%u (cMappedPages=%u)\n",
            GCPhys, g_apszPageStates[u2State], hrc, hrc, RTNtLastStatusValue(), RTNtLastErrorValue(),
            pVM->nem.s.cMappedPages));

    PGMPhysNemEnumPagesByState(pVM, pVCpu, NEM_WIN_PAGE_STATE_READABLE, nemR3WinUnmapOnePageCallback, NULL);
    Log(("nemHCWinHandleMemoryAccessPageCheckerCallback: Unmapped all (cMappedPages=%u)\n", pVM->nem.s.cMappedPages));

    pState->fDidSomething = true;
    pState->fCanResume    = true;
    pInfo->u2NemState = NEM_WIN_PAGE_STATE_UNMAPPED;
    return VINF_SUCCESS;
#endif
}


#ifdef NEM_WIN_USE_OUR_OWN_RUN_API

# ifdef IN_RING0
/**
 * Wrapper around nemR0WinImportState that converts VERR_NEM_CHANGE_PGM_MODE and
 * VERR_NEM_FLUSH_TBL into informational status codes and logs+asserts statuses.
 *
 * @returns VBox strict status code.
 * @param   pGVM            The global (ring-0) VM structure.
 * @param   pGVCpu          The global (ring-0) per CPU structure.
 * @param   pCtx            The CPU context to import into.
 * @param   fWhat           What to import.
 * @param   pszCaller       Whoe is doing the importing.
 */
DECLINLINE(VBOXSTRICTRC) nemR0WinImportStateStrict(PGVM pGVM, PGVMCPU pGVCpu, PCPUMCTX pCtx, uint64_t fWhat, const char *pszCaller)
{
    int rc = nemR0WinImportState(pGVM, pGVCpu, pCtx, fWhat);
    if (RT_SUCCESS(rc))
    {
        Assert(rc == VINF_SUCCESS);
        return VINF_SUCCESS;
    }

    if (rc == VERR_NEM_CHANGE_PGM_MODE || rc == VERR_NEM_FLUSH_TLB)
    {
        Log4(("%s/%u: nemR0WinImportState -> %Rrc\n", pszCaller, pGVCpu->idCpu, -rc));
        return -rc;
    }
    RT_NOREF(pszCaller);
    AssertMsgFailedReturn(("%s/%u: nemR0WinImportState failed: %Rrc\n", pszCaller, pGVCpu->idCpu, rc), rc);
}
# endif /* IN_RING0 */

/**
 * Copies register state from the X64 intercept message header.
 *
 * ASSUMES no state copied yet.
 *
 * @param   pCtx            The registe rcontext.
 * @param   pHdr            The X64 intercept message header.
 */
DECLINLINE(void) nemHCWinCopyStateFromX64Header(PCPUMCTX pCtx, HV_X64_INTERCEPT_MESSAGE_HEADER const *pHdr)
{
    Assert(    (pCtx->fExtrn & (CPUMCTX_EXTRN_RIP | CPUMCTX_EXTRN_RFLAGS | CPUMCTX_EXTRN_CS))
            ==                 (CPUMCTX_EXTRN_RIP | CPUMCTX_EXTRN_RFLAGS | CPUMCTX_EXTRN_CS));
    NEM_WIN_COPY_BACK_SEG(pCtx->cs, pHdr->CsSegment);
    pCtx->rip    = pHdr->Rip;
    pCtx->rflags.u = pHdr->Rflags;
    pCtx->fExtrn &= ~(CPUMCTX_EXTRN_RIP | CPUMCTX_EXTRN_RFLAGS | CPUMCTX_EXTRN_CS);
}


/**
 * Deals with memory intercept message.
 *
 * @returns Strict VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context per CPU structure.
 * @param   pMsg            The message.
 * @param   pCtx            The register context.
 * @param   pGVCpu          The global (ring-0) per CPU structure (NULL in r3).
 */
NEM_TMPL_STATIC VBOXSTRICTRC nemHCWinHandleMessageMemory(PVM pVM, PVMCPU pVCpu, HV_X64_MEMORY_INTERCEPT_MESSAGE const *pMsg,
                                                         PCPUMCTX pCtx, PGVMCPU pGVCpu)
{
    Assert(   pMsg->Header.InterceptAccessType == HV_INTERCEPT_ACCESS_READ
           || pMsg->Header.InterceptAccessType == HV_INTERCEPT_ACCESS_WRITE
           || pMsg->Header.InterceptAccessType == HV_INTERCEPT_ACCESS_EXECUTE);

    /*
     * Whatever we do, we must clear pending event ejection upon resume.
     */
    if (pMsg->Header.ExecutionState.InterruptionPending)
        pCtx->fExtrn &= ~CPUMCTX_EXTRN_NEM_WIN_MASK;

    /*
     * Ask PGM for information about the given GCPhys.  We need to check if we're
     * out of sync first.
     */
    NEMHCWINHMACPCCSTATE State = { pMsg->Header.InterceptAccessType == HV_INTERCEPT_ACCESS_WRITE, false, false };
    PGMPHYSNEMPAGEINFO   Info;
    int rc = PGMPhysNemPageInfoChecker(pVM, pVCpu, pMsg->GuestPhysicalAddress, State.fWriteAccess, &Info,
                                       nemHCWinHandleMemoryAccessPageCheckerCallback, &State);
    if (RT_SUCCESS(rc))
    {
        if (Info.fNemProt & (  pMsg->Header.InterceptAccessType == HV_INTERCEPT_ACCESS_WRITE
                             ? NEM_PAGE_PROT_WRITE : NEM_PAGE_PROT_READ))
        {
            if (State.fCanResume)
            {
                Log4(("MemExit/%u: %04x:%08RX64: %RGp (=>%RHp) %s fProt=%u%s%s%s; restarting (%s)\n",
                      pVCpu->idCpu, pMsg->Header.CsSegment.Selector, pMsg->Header.Rip,
                      pMsg->GuestPhysicalAddress, Info.HCPhys, g_apszPageStates[Info.u2NemState], Info.fNemProt,
                      Info.fHasHandlers ? " handlers" : "", Info.fZeroPage    ? " zero-pg" : "",
                      State.fDidSomething ? "" : " no-change", g_apszHvInterceptAccessTypes[pMsg->Header.InterceptAccessType]));
                return VINF_SUCCESS;
            }
        }
        Log4(("MemExit/%u: %04x:%08RX64: %RGp (=>%RHp) %s fProt=%u%s%s%s; emulating (%s)\n",
              pVCpu->idCpu, pMsg->Header.CsSegment.Selector, pMsg->Header.Rip,
              pMsg->GuestPhysicalAddress, Info.HCPhys, g_apszPageStates[Info.u2NemState], Info.fNemProt,
              Info.fHasHandlers ? " handlers" : "", Info.fZeroPage    ? " zero-pg" : "",
              State.fDidSomething ? "" : " no-change", g_apszHvInterceptAccessTypes[pMsg->Header.InterceptAccessType]));
    }
    else
        Log4(("MemExit/%u: %04x:%08RX64: %RGp rc=%Rrc%s; emulating (%s)\n",
              pVCpu->idCpu, pMsg->Header.CsSegment.Selector, pMsg->Header.Rip, pMsg->GuestPhysicalAddress, rc,
              State.fDidSomething ? " modified-backing" : "", g_apszHvInterceptAccessTypes[pMsg->Header.InterceptAccessType]));

    /*
     * Emulate the memory access, either access handler or special memory.
     */
    nemHCWinCopyStateFromX64Header(pCtx, &pMsg->Header);
    VBOXSTRICTRC rcStrict;
# ifdef IN_RING0
    rcStrict = nemR0WinImportStateStrict(pGVCpu->pGVM, pGVCpu, pCtx, NEM_WIN_CPUMCTX_EXTRN_MASK_FOR_IEM, "MemExit");
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;
# else
    rc = nemHCWinCopyStateFromHyperV(pVM, pVCpu, pCtx, NEM_WIN_CPUMCTX_EXTRN_MASK_FOR_IEM);
    AssertRCReturn(rc, rc);
    NOREF(pGVCpu);
# endif

    if (pMsg->InstructionByteCount > 0)
        rcStrict = IEMExecOneWithPrefetchedByPC(pVCpu, CPUMCTX2CORE(pCtx), pMsg->Header.Rip,
                                                pMsg->InstructionBytes, pMsg->InstructionByteCount);
    else
        rcStrict = IEMExecOne(pVCpu);
    /** @todo do we need to do anything wrt debugging here?   */
    return rcStrict;

}


/**
 * Deals with I/O port intercept message.
 *
 * @returns Strict VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context per CPU structure.
 * @param   pMsg            The message.
 * @param   pGVCpu          The global (ring-0) per CPU structure (NULL in r3).
 */
NEM_TMPL_STATIC VBOXSTRICTRC nemHCWinHandleMessageIoPort(PVM pVM, PVMCPU pVCpu, HV_X64_IO_PORT_INTERCEPT_MESSAGE const *pMsg,
                                                         PCPUMCTX pCtx, PGVMCPU pGVCpu)
{
    Assert(   pMsg->AccessInfo.AccessSize == 1
           || pMsg->AccessInfo.AccessSize == 2
           || pMsg->AccessInfo.AccessSize == 4);
    Assert(   pMsg->Header.InterceptAccessType == HV_INTERCEPT_ACCESS_READ
           || pMsg->Header.InterceptAccessType == HV_INTERCEPT_ACCESS_WRITE);

    /*
     * Whatever we do, we must clear pending event ejection upon resume.
     */
    if (pMsg->Header.ExecutionState.InterruptionPending)
        pCtx->fExtrn &= ~CPUMCTX_EXTRN_NEM_WIN_MASK;

    VBOXSTRICTRC rcStrict;
    if (!pMsg->AccessInfo.StringOp)
    {
        /*
         * Simple port I/O.
         */
        static uint32_t const s_fAndMask[8] =
        {   UINT32_MAX, UINT32_C(0xff), UINT32_C(0xffff), UINT32_MAX,   UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX   };
        uint32_t const        fAndMask      = s_fAndMask[pMsg->AccessInfo.AccessSize];
        if (pMsg->Header.InterceptAccessType == HV_INTERCEPT_ACCESS_WRITE)
        {
            rcStrict = IOMIOPortWrite(pVM, pVCpu, pMsg->PortNumber, (uint32_t)pMsg->Rax & fAndMask, pMsg->AccessInfo.AccessSize);
            Log4(("IOExit/%u: %04x:%08RX64: OUT %#x, %#x LB %u rcStrict=%Rrc\n",
                  pVCpu->idCpu, pMsg->Header.CsSegment.Selector, pMsg->Header.Rip, pMsg->PortNumber,
                  (uint32_t)pMsg->Rax & fAndMask, pMsg->AccessInfo.AccessSize, VBOXSTRICTRC_VAL(rcStrict) ));
            if (IOM_SUCCESS(rcStrict))
            {
                nemHCWinCopyStateFromX64Header(pCtx, &pMsg->Header);
                nemHCWinAdvanceGuestRipAndClearRF(pVCpu, pCtx, &pMsg->Header);
            }
        }
        else
        {
            uint32_t uValue = 0;
            rcStrict = IOMIOPortRead(pVM, pVCpu, pMsg->PortNumber, &uValue, pMsg->AccessInfo.AccessSize);
            Log4(("IOExit/%u: %04x:%08RX64: IN %#x LB %u -> %#x, rcStrict=%Rrc\n", pVCpu->idCpu, pMsg->Header.CsSegment.Selector,
                  pMsg->Header.Rip, pMsg->PortNumber, pMsg->AccessInfo.AccessSize, uValue, VBOXSTRICTRC_VAL(rcStrict) ));
            if (IOM_SUCCESS(rcStrict))
            {
                if (pMsg->AccessInfo.AccessSize != 4)
                    pCtx->rax = (pMsg->Rax & ~(uint64_t)fAndMask) | (uValue & fAndMask);
                else
                    pCtx->rax = uValue;
                pCtx->fExtrn &= ~CPUMCTX_EXTRN_RAX;
                Log4(("IOExit/%u: RAX %#RX64 -> %#RX64\n", pVCpu->idCpu, pMsg->Rax, pCtx->rax));
                nemHCWinCopyStateFromX64Header(pCtx, &pMsg->Header);
                nemHCWinAdvanceGuestRipAndClearRF(pVCpu, pCtx, &pMsg->Header);
            }
        }
    }
    else
    {
        /*
         * String port I/O.
         */
        /** @todo Someone at Microsoft please explain how we can get the address mode
         * from the IoPortAccess.VpContext.  CS.Attributes is only sufficient for
         * getting the default mode, it can always be overridden by a prefix.   This
         * forces us to interpret the instruction from opcodes, which is suboptimal.
         * Both AMD-V and VT-x includes the address size in the exit info, at least on
         * CPUs that are reasonably new.
         *
         * Of course, it's possible this is an undocumented and we just need to do some
         * experiments to figure out how it's communicated.  Alternatively, we can scan
         * the opcode bytes for possible evil prefixes.
         */
        nemHCWinCopyStateFromX64Header(pCtx, &pMsg->Header);
        pCtx->fExtrn &= ~(  CPUMCTX_EXTRN_RAX | CPUMCTX_EXTRN_RCX | CPUMCTX_EXTRN_RDI | CPUMCTX_EXTRN_RSI
                          | CPUMCTX_EXTRN_DS  | CPUMCTX_EXTRN_ES);
        NEM_WIN_COPY_BACK_SEG(pCtx->ds, pMsg->DsSegment);
        NEM_WIN_COPY_BACK_SEG(pCtx->es, pMsg->EsSegment);
        pCtx->rax = pMsg->Rax;
        pCtx->rcx = pMsg->Rcx;
        pCtx->rdi = pMsg->Rdi;
        pCtx->rsi = pMsg->Rsi;
# ifdef IN_RING0
        rcStrict = nemR0WinImportStateStrict(pGVCpu->pGVM, pGVCpu, pCtx, NEM_WIN_CPUMCTX_EXTRN_MASK_FOR_IEM, "IOExit");
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
# else
        int rc = nemHCWinCopyStateFromHyperV(pVM, pVCpu, pCtx, NEM_WIN_CPUMCTX_EXTRN_MASK_FOR_IEM);
        AssertRCReturn(rc, rc);
        RT_NOREF(pGVCpu);
# endif

        Log4(("IOExit/%u: %04x:%08RX64: %s%s %#x LB %u (emulating)\n",
              pVCpu->idCpu, pMsg->Header.CsSegment.Selector, pMsg->Header.Rip,
              pMsg->AccessInfo.RepPrefix ? "REP " : "",
              pMsg->Header.InterceptAccessType == HV_INTERCEPT_ACCESS_WRITE ? "OUTS" : "INS",
              pMsg->PortNumber, pMsg->AccessInfo.AccessSize ));
        rcStrict = IEMExecOne(pVCpu);
    }
    if (IOM_SUCCESS(rcStrict))
    {
        /*
         * Do debug checks.
         */
        if (   pMsg->Header.ExecutionState.DebugActive /** @todo Microsoft: Does DebugActive this only reflext DR7? */
            || (pMsg->Header.Rflags & X86_EFL_TF)
            || DBGFBpIsHwIoArmed(pVM) )
        {
            /** @todo Debugging. */
        }
    }
    return rcStrict;
}


/**
 * Handles messages (VM exits).
 *
 * @returns Strict VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context per CPU structure.
 * @param   pMappingHeader  The message slot mapping.
 * @param   pCtx            The register context.
 * @param   pGVCpu          The global (ring-0) per CPU structure (NULL in r3).
 */
NEM_TMPL_STATIC VBOXSTRICTRC nemHCWinHandleMessage(PVM pVM, PVMCPU pVCpu, VID_MESSAGE_MAPPING_HEADER volatile *pMappingHeader,
                                                   PCPUMCTX pCtx, PGVMCPU pGVCpu)
{
    if (pMappingHeader->enmVidMsgType == VidMessageHypervisorMessage)
    {
        AssertMsg(pMappingHeader->cbMessage == HV_MESSAGE_SIZE, ("%#x\n", pMappingHeader->cbMessage));
        HV_MESSAGE const *pMsg = (HV_MESSAGE const *)(pMappingHeader + 1);
        switch (pMsg->Header.MessageType)
        {
            case HvMessageTypeUnmappedGpa:
                Assert(pMsg->Header.PayloadSize == RT_UOFFSETOF(HV_X64_MEMORY_INTERCEPT_MESSAGE, DsSegment));
                STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitMemUnmapped);
                return nemHCWinHandleMessageMemory(pVM, pVCpu, &pMsg->X64MemoryIntercept, pCtx, pGVCpu);

            case HvMessageTypeGpaIntercept:
                Assert(pMsg->Header.PayloadSize == RT_UOFFSETOF(HV_X64_MEMORY_INTERCEPT_MESSAGE, DsSegment));
                STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitMemIntercept);
                return nemHCWinHandleMessageMemory(pVM, pVCpu, &pMsg->X64MemoryIntercept, pCtx, pGVCpu);

            case HvMessageTypeX64IoPortIntercept:
                Assert(pMsg->Header.PayloadSize == sizeof(pMsg->X64IoPortIntercept));
                STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitPortIo);
                return nemHCWinHandleMessageIoPort(pVM, pVCpu, &pMsg->X64IoPortIntercept, pCtx, pGVCpu);

            case HvMessageTypeX64Halt:
                STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitHalt);
                return VINF_EM_HALT;

            case HvMessageTypeX64InterruptWindow:
                AssertLogRelMsgFailedReturn(("Message type %#x not implemented!\n", pMsg->Header.MessageType),
                                            VERR_INTERNAL_ERROR_2);

            case HvMessageTypeInvalidVpRegisterValue:
            case HvMessageTypeUnrecoverableException:
            case HvMessageTypeUnsupportedFeature:
            case HvMessageTypeTlbPageSizeMismatch:
                AssertLogRelMsgFailedReturn(("Message type %#x not implemented!\n", pMsg->Header.MessageType),
                                            VERR_INTERNAL_ERROR_2);

            case HvMessageTypeX64MsrIntercept:
            case HvMessageTypeX64CpuidIntercept:
            case HvMessageTypeX64ExceptionIntercept:
            case HvMessageTypeX64ApicEoi:
            case HvMessageTypeX64LegacyFpError:
            case HvMessageTypeX64RegisterIntercept:
            case HvMessageTypeApicEoi:
            case HvMessageTypeFerrAsserted:
            case HvMessageTypeEventLogBufferComplete:
            case HvMessageTimerExpired:
                AssertLogRelMsgFailedReturn(("Unexpected message on CPU #%u: #x\n", pVCpu->idCpu, pMsg->Header.MessageType),
                                            VERR_INTERNAL_ERROR_2);

            default:
                AssertLogRelMsgFailedReturn(("Unknown message on CPU #%u: #x\n", pVCpu->idCpu, pMsg->Header.MessageType),
                                            VERR_INTERNAL_ERROR_2);
        }
    }
    else
        AssertLogRelMsgFailedReturn(("Unexpected VID message type on CPU #%u: %#x LB %u\n",
                                     pVCpu->idCpu, pMappingHeader->enmVidMsgType, pMappingHeader->cbMessage),
                                    VERR_INTERNAL_ERROR_3);
}


/**
 * Worker for nemHCWinRunGC that stops the execution on the way out.
 *
 * The CPU was running the last time we checked, no there are no messages that
 * needs being marked handled/whatever.  Caller checks this.
 *
 * @returns rcStrict on success, error status on failure.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context per CPU structure.
 * @param   rcStrict        The nemHCWinRunGC return status.  This is a little
 *                          bit unnecessary, except in internal error cases,
 *                          since we won't need to stop the CPU if we took an
 *                          exit.
 * @param   pMappingHeader  The message slot mapping.
 * @param   pGVM            The global (ring-0) VM structure (NULL in r3).
 * @param   pGVCpu          The global (ring-0) per CPU structure (NULL in r3).
 */
NEM_TMPL_STATIC VBOXSTRICTRC nemHCWinStopCpu(PVM pVM, PVMCPU pVCpu, VBOXSTRICTRC rcStrict,
                                             VID_MESSAGE_MAPPING_HEADER volatile *pMappingHeader,
                                             PGVM pGVM, PGVMCPU pGVCpu)
{
    /*
     * Try stopping the processor.  If we're lucky we manage to do this before it
     * does another VM exit.
     */
# ifdef IN_RING0
    pVCpu->nem.s.uIoCtlBuf.idCpu = pGVCpu->idCpu;
    NTSTATUS rcNt = nemR0NtPerformIoControl(pGVM, pGVM->nem.s.IoCtlStopVirtualProcessor.uFunction,
                                            &pVCpu->nem.s.uIoCtlBuf.idCpu, sizeof(pVCpu->nem.s.uIoCtlBuf.idCpu),
                                            NULL, 0);
    if (NT_SUCCESS(rcNt))
    {
        Log8(("nemHCWinStopCpu: Stopping CPU succeeded (cpu status %u)\n", nemHCWinCpuGetRunningStatus(pVCpu) ));
        STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatStopCpuSuccess);
        return rcStrict;
    }
# else
    BOOL fRet = VidStopVirtualProcessor(pVM->nem.s.hPartitionDevice, pVCpu->idCpu);
    if (fRet)
    {
        Log8(("nemHCWinStopCpu: Stopping CPU succeeded (cpu status %u)\n", nemHCWinCpuGetRunningStatus(pVCpu) ));
        STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatStopCpuSuccess);
        return rcStrict;
    }
    RT_NOREF(pGVM, pGVCpu);
# endif

    /*
     * Dang. The CPU stopped by itself and we got a couple of message to deal with.
     */
# ifdef IN_RING0
    AssertLogRelMsgReturn(rcNt == ERROR_VID_STOP_PENDING, ("rcNt=%#x\n", rcNt),
                          RT_SUCCESS(rcStrict) ?  VERR_INTERNAL_ERROR_3 : rcStrict);
# else
    DWORD dwErr = RTNtLastErrorValue();
    AssertLogRelMsgReturn(dwErr == ERROR_VID_STOP_PENDING, ("dwErr=%#u (%#x)\n", dwErr, dwErr),
                          RT_SUCCESS(rcStrict) ?  VERR_INTERNAL_ERROR_3 : rcStrict);
# endif
    Log8(("nemHCWinStopCpu: Stopping CPU pending...\n"));
    STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatStopCpuPending);

    /*
     * First message: Exit or similar.
     * Note! We can safely ASSUME that rcStrict isn't an important information one.
     */
# ifdef IN_RING0
    pVCpu->nem.s.uIoCtlBuf.MsgSlotHandleAndGetNext.iCpu     = pGVCpu->idCpu;
    pVCpu->nem.s.uIoCtlBuf.MsgSlotHandleAndGetNext.fFlags   = VID_MSHAGN_F_GET_NEXT_MESSAGE;
    pVCpu->nem.s.uIoCtlBuf.MsgSlotHandleAndGetNext.cMillies = 30000; /*ms*/
    rcNt = nemR0NtPerformIoControl(pGVM, pGVM->nem.s.IoCtlMessageSlotHandleAndGetNext.uFunction,
                                   &pVCpu->nem.s.uIoCtlBuf.MsgSlotHandleAndGetNext,
                                   sizeof(pVCpu->nem.s.uIoCtlBuf.MsgSlotHandleAndGetNext),
                                   NULL, 0);
    AssertLogRelMsgReturn(NT_SUCCESS(rcNt), ("1st VidMessageSlotHandleAndGetNext after ERROR_VID_STOP_PENDING failed: %#x\n", rcNt),
                          RT_SUCCESS(rcStrict) ? VERR_INTERNAL_ERROR_3 : rcStrict);
# else
    BOOL fWait = g_pfnVidMessageSlotHandleAndGetNext(pVM->nem.s.hPartitionDevice, pVCpu->idCpu,
                                                     VID_MSHAGN_F_GET_NEXT_MESSAGE, 30000 /*ms*/);
    AssertLogRelMsgReturn(fWait, ("1st VidMessageSlotHandleAndGetNext after ERROR_VID_STOP_PENDING failed: %u\n", RTNtLastErrorValue()),
                          RT_SUCCESS(rcStrict) ? VERR_INTERNAL_ERROR_3 : rcStrict);
# endif

    /* It should be a hypervisor message and definitely not a stop request completed message. */
    VID_MESSAGE_TYPE enmVidMsgType = pMappingHeader->enmVidMsgType;
    AssertLogRelMsgReturn(enmVidMsgType != VidMessageStopRequestComplete,
                          ("Unexpected 1st message following ERROR_VID_STOP_PENDING: %#x LB %#x\n",
                           enmVidMsgType, pMappingHeader->cbMessage),
                          RT_SUCCESS(rcStrict) ? VERR_INTERNAL_ERROR_3 : rcStrict);

    VBOXSTRICTRC rcStrict2 = nemHCWinHandleMessage(pVM, pVCpu, pMappingHeader, CPUMQueryGuestCtxPtr(pVCpu), pGVCpu);
    if (rcStrict2 != VINF_SUCCESS && RT_SUCCESS(rcStrict))
        rcStrict = rcStrict2;

    /*
     * Mark it as handled and get the stop request completed message, then mark
     * that as handled too.  CPU is back into fully stopped stated then.
     */
# ifdef IN_RING0
    pVCpu->nem.s.uIoCtlBuf.MsgSlotHandleAndGetNext.iCpu     = pGVCpu->idCpu;
    pVCpu->nem.s.uIoCtlBuf.MsgSlotHandleAndGetNext.fFlags   = VID_MSHAGN_F_HANDLE_MESSAGE | VID_MSHAGN_F_GET_NEXT_MESSAGE;
    pVCpu->nem.s.uIoCtlBuf.MsgSlotHandleAndGetNext.cMillies = 30000; /*ms*/
    rcNt = nemR0NtPerformIoControl(pGVM, pGVM->nem.s.IoCtlMessageSlotHandleAndGetNext.uFunction,
                                   &pVCpu->nem.s.uIoCtlBuf.MsgSlotHandleAndGetNext,
                                   sizeof(pVCpu->nem.s.uIoCtlBuf.MsgSlotHandleAndGetNext),
                                   NULL, 0);
    AssertLogRelMsgReturn(NT_SUCCESS(rcNt), ("2st VidMessageSlotHandleAndGetNext after ERROR_VID_STOP_PENDING failed: %#x\n", rcNt),
                          RT_SUCCESS(rcStrict) ? VERR_INTERNAL_ERROR_3 : rcStrict);
# else
    fWait = g_pfnVidMessageSlotHandleAndGetNext(pVM->nem.s.hPartitionDevice, pVCpu->idCpu,
                                                VID_MSHAGN_F_HANDLE_MESSAGE | VID_MSHAGN_F_GET_NEXT_MESSAGE, 30000 /*ms*/);
    AssertLogRelMsgReturn(fWait, ("2nd VidMessageSlotHandleAndGetNext after ERROR_VID_STOP_PENDING failed: %u\n", RTNtLastErrorValue()),
                          RT_SUCCESS(rcStrict) ? VERR_INTERNAL_ERROR_3 : rcStrict);
# endif

    /* It should be a stop request completed message. */
    enmVidMsgType = pMappingHeader->enmVidMsgType;
    AssertLogRelMsgReturn(enmVidMsgType == VidMessageStopRequestComplete,
                          ("Unexpected 2nd message following ERROR_VID_STOP_PENDING: %#x LB %#x\n",
                           enmVidMsgType, pMappingHeader->cbMessage),
                          RT_SUCCESS(rcStrict) ? VERR_INTERNAL_ERROR_3 : rcStrict);

    /* Mark this as handled. */
# ifdef IN_RING0
    pVCpu->nem.s.uIoCtlBuf.MsgSlotHandleAndGetNext.iCpu     = pGVCpu->idCpu;
    pVCpu->nem.s.uIoCtlBuf.MsgSlotHandleAndGetNext.fFlags   = VID_MSHAGN_F_HANDLE_MESSAGE;
    pVCpu->nem.s.uIoCtlBuf.MsgSlotHandleAndGetNext.cMillies = 30000; /*ms*/
    rcNt = nemR0NtPerformIoControl(pGVM, pGVM->nem.s.IoCtlMessageSlotHandleAndGetNext.uFunction,
                                   &pVCpu->nem.s.uIoCtlBuf.MsgSlotHandleAndGetNext,
                                   sizeof(pVCpu->nem.s.uIoCtlBuf.MsgSlotHandleAndGetNext),
                                   NULL, 0);
    AssertLogRelMsgReturn(NT_SUCCESS(rcNt), ("3rd VidMessageSlotHandleAndGetNext after ERROR_VID_STOP_PENDING failed: %#x\n", rcNt),
                          RT_SUCCESS(rcStrict) ? VERR_INTERNAL_ERROR_3 : rcStrict);
# else
    fWait = g_pfnVidMessageSlotHandleAndGetNext(pVM->nem.s.hPartitionDevice, pVCpu->idCpu, VID_MSHAGN_F_HANDLE_MESSAGE, 30000 /*ms*/);
    AssertLogRelMsgReturn(fWait, ("3rd VidMessageSlotHandleAndGetNext after ERROR_VID_STOP_PENDING failed: %u\n", RTNtLastErrorValue()),
                          RT_SUCCESS(rcStrict) ? VERR_INTERNAL_ERROR_3 : rcStrict);
# endif
    Log8(("nemHCWinStopCpu: Stopped the CPU (rcStrict=%Rrc)\n", VBOXSTRICTRC_VAL(rcStrict) ));
    return rcStrict;
}


NEM_TMPL_STATIC VBOXSTRICTRC nemHCWinRunGC(PVM pVM, PVMCPU pVCpu, PGVM pGVM, PGVMCPU pGVCpu)
{
    PCPUMCTX pCtx = CPUMQueryGuestCtxPtr(pVCpu);
    LogFlow(("NEM/%u: %04x:%08RX64 efl=%#08RX64 <=\n", pVCpu->idCpu, pCtx->cs.Sel, pCtx->rip, pCtx->rflags));
# ifdef LOG_ENABLED
    if (LogIs3Enabled())
        nemHCWinLogState(pVM, pVCpu);
# endif

    /*
     * Try switch to NEM runloop state.
     */
    if (VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC_NEM, VMCPUSTATE_STARTED))
    { /* likely */ }
    else
    {
        VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC_NEM, VMCPUSTATE_STARTED_EXEC_NEM_CANCELED);
        LogFlow(("NEM/%u: returning immediately because canceled\n", pVCpu->idCpu));
        return VINF_SUCCESS;
    }

    /*
     * The run loop.
     *
     * Current approach to state updating to use the sledgehammer and sync
     * everything every time.  This will be optimized later.
     */
    VID_MESSAGE_MAPPING_HEADER volatile *pMappingHeader = (VID_MESSAGE_MAPPING_HEADER volatile *)pVCpu->nem.s.pvMsgSlotMapping;
    uint32_t     cMillies           = 5000; /** @todo lower this later... */
    const bool   fSingleStepping    = false; /** @todo get this from somewhere. */
    VBOXSTRICTRC rcStrict           = VINF_SUCCESS;
    for (unsigned iLoop = 0;;iLoop++)
    {
        /*
         * Ensure that hyper-V has the whole state.
         */
        if ((pCtx->fExtrn & (CPUMCTX_EXTRN_ALL | CPUMCTX_EXTRN_NEM_WIN_MASK)) != (CPUMCTX_EXTRN_ALL | CPUMCTX_EXTRN_NEM_WIN_MASK))
        {
# ifdef IN_RING0
            int rc2 = nemR0WinExportState(pGVM, pGVCpu, pCtx);
# else
            int rc2 = nemHCWinCopyStateToHyperV(pVM, pVCpu, pCtx);
            RT_NOREF(pGVM, pGVCpu);
# endif
            AssertRCReturn(rc2, rc2);
        }

        /*
         * Run a bit.
         */
        if (   !VM_FF_IS_PENDING(pVM, VM_FF_EMT_RENDEZVOUS | VM_FF_TM_VIRTUAL_SYNC)
            && !VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_TO_R3_MASK))
        {
            if (pVCpu->nem.s.fHandleAndGetFlags)
            { /* Very likely that the CPU does NOT need starting (pending msg, running). */ }
            else
            {
# ifdef IN_RING0
                pVCpu->nem.s.uIoCtlBuf.idCpu = pGVCpu->idCpu;
                NTSTATUS rcNt = nemR0NtPerformIoControl(pGVM, pGVM->nem.s.IoCtlStartVirtualProcessor.uFunction,
                                                        &pVCpu->nem.s.uIoCtlBuf.idCpu, sizeof(pVCpu->nem.s.uIoCtlBuf.idCpu),
                                                        NULL, 0);
                LogFlow(("NEM/%u: IoCtlStartVirtualProcessor -> %#x\n", pVCpu->idCpu, rcNt));
                AssertLogRelMsgReturn(NT_SUCCESS(rcNt), ("VidStartVirtualProcessor failed for CPU #%u: %#x\n", pGVCpu->idCpu, rcNt),
                                      VERR_INTERNAL_ERROR_3);
# else
                AssertLogRelMsgReturn(g_pfnVidStartVirtualProcessor(pVM->nem.s.hPartitionDevice, pVCpu->idCpu),
                                      ("VidStartVirtualProcessor failed for CPU #%u: %u (%#x, rcNt=%#x)\n",
                                       pVCpu->idCpu, RTNtLastErrorValue(), RTNtLastErrorValue(), RTNtLastStatusValue()),
                                      VERR_INTERNAL_ERROR_3);
# endif
                pVCpu->nem.s.fHandleAndGetFlags = VID_MSHAGN_F_GET_NEXT_MESSAGE;
            }

            if (VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC_NEM_WAIT, VMCPUSTATE_STARTED_EXEC_NEM))
            {
# ifdef IN_RING0
                pVCpu->nem.s.uIoCtlBuf.MsgSlotHandleAndGetNext.iCpu     = pGVCpu->idCpu;
                pVCpu->nem.s.uIoCtlBuf.MsgSlotHandleAndGetNext.fFlags   = pVCpu->nem.s.fHandleAndGetFlags;
                pVCpu->nem.s.uIoCtlBuf.MsgSlotHandleAndGetNext.cMillies = cMillies;
                NTSTATUS rcNt = nemR0NtPerformIoControl(pGVM, pGVM->nem.s.IoCtlMessageSlotHandleAndGetNext.uFunction,
                                                        &pVCpu->nem.s.uIoCtlBuf.MsgSlotHandleAndGetNext,
                                                        sizeof(pVCpu->nem.s.uIoCtlBuf.MsgSlotHandleAndGetNext),
                                                        NULL, 0);
                VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC_NEM, VMCPUSTATE_STARTED_EXEC_NEM_WAIT);
                if (rcNt == STATUS_SUCCESS)
# else
                BOOL fRet = VidMessageSlotHandleAndGetNext(pVM->nem.s.hPartitionDevice, pVCpu->idCpu,
                                                           pVCpu->nem.s.fHandleAndGetFlags, cMillies);
                VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC_NEM, VMCPUSTATE_STARTED_EXEC_NEM_WAIT);
                if (fRet)
# endif
                {
                    /*
                     * Deal with the message.
                     */
                    rcStrict = nemHCWinHandleMessage(pVM, pVCpu, pMappingHeader, pCtx, pGVCpu);
                    pVCpu->nem.s.fHandleAndGetFlags |= VID_MSHAGN_F_HANDLE_MESSAGE;
                    if (rcStrict == VINF_SUCCESS)
                    { /* hopefully likely */ }
                    else
                    {
                        LogFlow(("NEM/%u: breaking: nemHCWinHandleMessage -> %Rrc\n", pVCpu->idCpu, VBOXSTRICTRC_VAL(rcStrict) ));
                        STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatBreakOnStatus);
                        break;
                    }
                }
                else
                {
                    /* VID.SYS merges STATUS_ALERTED and STATUS_USER_APC into STATUS_TIMEOUT,
                       so after NtAlertThread we end up here with a STATUS_TIMEOUT.  And yeah,
                       the error code conversion is into WAIT_XXX, i.e. NT status codes. */
# ifndef IN_RING0
                    DWORD rcNt = GetLastError();
# endif
                    LogFlow(("NEM/%u: VidMessageSlotHandleAndGetNext -> %#x\n", pVCpu->idCpu, rcNt));
                    AssertLogRelMsgReturn(   rcNt == STATUS_TIMEOUT
                                          || rcNt == STATUS_ALERTED  /* just in case */
                                          || rcNt == STATUS_USER_APC /* ditto */
                                          , ("VidMessageSlotHandleAndGetNext failed for CPU #%u: %#x (%u)\n", pVCpu->idCpu, rcNt, rcNt),
                                          VERR_INTERNAL_ERROR_3);
                    pVCpu->nem.s.fHandleAndGetFlags = VID_MSHAGN_F_GET_NEXT_MESSAGE;
                    STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatGetMsgTimeout);
                }

                /*
                 * If no relevant FFs are pending, loop.
                 */
                if (   !VM_FF_IS_PENDING(   pVM,   !fSingleStepping ? VM_FF_HP_R0_PRE_HM_MASK    : VM_FF_HP_R0_PRE_HM_STEP_MASK)
                    && !VMCPU_FF_IS_PENDING(pVCpu, !fSingleStepping ? VMCPU_FF_HP_R0_PRE_HM_MASK : VMCPU_FF_HP_R0_PRE_HM_STEP_MASK) )
                    continue;

                /** @todo Try handle pending flags, not just return to EM loops.  Take care
                 *        not to set important RCs here unless we've handled a message. */
                LogFlow(("NEM/%u: breaking: pending FF (%#x / %#x)\n",
                         pVCpu->idCpu, pVM->fGlobalForcedActions, pVCpu->fLocalForcedActions));
                STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatBreakOnFFPost);
            }
            else
            {
                LogFlow(("NEM/%u: breaking: canceled %d (pre exec)\n", pVCpu->idCpu, VMCPU_GET_STATE(pVCpu) ));
                STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatBreakOnCancel);
            }
        }
        else
        {
            LogFlow(("NEM/%u: breaking: pending FF (pre exec)\n", pVCpu->idCpu));
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatBreakOnFFPre);
        }
        break;
    } /* the run loop */


    /*
     * If the CPU is running, make sure to stop it before we try sync back the
     * state and return to EM.
     */
    if (pVCpu->nem.s.fHandleAndGetFlags == VID_MSHAGN_F_GET_NEXT_MESSAGE)
    {
        pVCpu->nem.s.fHandleAndGetFlags = 0;
        rcStrict = nemHCWinStopCpu(pVM, pVCpu, rcStrict, pMappingHeader, pGVM, pGVCpu);
    }

    if (!VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED, VMCPUSTATE_STARTED_EXEC_NEM))
        VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED, VMCPUSTATE_STARTED_EXEC_NEM_CANCELED);

    if (pCtx->fExtrn & (CPUMCTX_EXTRN_ALL | (CPUMCTX_EXTRN_NEM_WIN_MASK & ~CPUMCTX_EXTRN_NEM_WIN_EVENT_INJECT)))
    {
# ifdef IN_RING0
        int rc2 = nemR0WinImportState(pGVM, pGVCpu, pCtx, CPUMCTX_EXTRN_ALL | CPUMCTX_EXTRN_NEM_WIN_MASK);
        if (RT_SUCCESS(rc2))
            pCtx->fExtrn = 0;
        else if (rc2 == VERR_NEM_CHANGE_PGM_MODE || rc2 == VERR_NEM_FLUSH_TLB)
        {
            pCtx->fExtrn = 0;
            if (rcStrict == VINF_SUCCESS || rcStrict == -rc2)
                rcStrict = -rc2;
            else
            {
                pVCpu->nem.s.rcPgmPending = -rc2;
                LogFlow(("NEM/%u: rcPgmPending=%Rrc (rcStrict=%Rrc)\n", pVCpu->idCpu, rc2, VBOXSTRICTRC_VAL(rcStrict) ));
            }
        }
# else
        int rc2 = nemHCWinCopyStateFromHyperV(pVM, pVCpu, pCtx, CPUMCTX_EXTRN_ALL | CPUMCTX_EXTRN_NEM_WIN_MASK);
        if (RT_SUCCESS(rc2))
            pCtx->fExtrn = 0;
# endif
        else if (RT_SUCCESS(rcStrict))
            rcStrict = rc2;
    }
    else
        pCtx->fExtrn = 0;

    LogFlow(("NEM/%u: %04x:%08RX64 efl=%#08RX64 => %Rrc\n",
             pVCpu->idCpu, pCtx->cs.Sel, pCtx->rip, pCtx->rflags, VBOXSTRICTRC_VAL(rcStrict) ));
    return rcStrict;
}

#endif /* NEM_WIN_USE_OUR_OWN_RUN_API */


/**
 * @callback_method_impl{FNPGMPHYSNEMCHECKPAGE}
 */
NEM_TMPL_STATIC DECLCALLBACK(int) nemHCWinUnsetForA20CheckerCallback(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhys,
                                                                     PPGMPHYSNEMPAGEINFO pInfo, void *pvUser)
{
    /* We'll just unmap the memory. */
    if (pInfo->u2NemState > NEM_WIN_PAGE_STATE_UNMAPPED)
    {
#ifdef NEM_WIN_USE_HYPERCALLS_FOR_PAGES
        int rc = nemHCWinHypercallUnmapPage(pVM, pVCpu, GCPhys);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
#else
        HRESULT hrc = WHvUnmapGpaRange(pVM->nem.s.hPartition, GCPhys, X86_PAGE_SIZE);
        if (SUCCEEDED(hrc))
#endif
        {
            uint32_t cMappedPages = ASMAtomicDecU32(&pVM->nem.s.cMappedPages); NOREF(cMappedPages);
            Log5(("NEM GPA unmapped/A20: %RGp (was %s, cMappedPages=%u)\n", GCPhys, g_apszPageStates[pInfo->u2NemState], cMappedPages));
            pInfo->u2NemState = NEM_WIN_PAGE_STATE_UNMAPPED;
        }
        else
        {
#ifdef NEM_WIN_USE_HYPERCALLS_FOR_PAGES
            LogRel(("nemHCWinUnsetForA20CheckerCallback/unmap: GCPhys=%RGp rc=%Rrc\n", GCPhys, rc));
            return rc;
#else
            LogRel(("nemHCWinUnsetForA20CheckerCallback/unmap: GCPhys=%RGp hrc=%Rhrc (%#x) Last=%#x/%u\n",
                    GCPhys, hrc, hrc, RTNtLastStatusValue(), RTNtLastErrorValue()));
            return VERR_INTERNAL_ERROR_2;
#endif
        }
    }
    RT_NOREF(pVCpu, pvUser);
    return VINF_SUCCESS;
}


/**
 * Unmaps a page from Hyper-V for the purpose of emulating A20 gate behavior.
 *
 * @returns The PGMPhysNemQueryPageInfo result.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   GCPhys          The page to unmap.
 */
NEM_TMPL_STATIC int nemHCWinUnmapPageForA20Gate(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhys)
{
    PGMPHYSNEMPAGEINFO Info;
    return PGMPhysNemPageInfoChecker(pVM, pVCpu, GCPhys, false /*fMakeWritable*/, &Info,
                                     nemHCWinUnsetForA20CheckerCallback, NULL);
}


void nemHCNativeNotifyHandlerPhysicalRegister(PVM pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhys, RTGCPHYS cb)
{
    Log5(("nemHCNativeNotifyHandlerPhysicalRegister: %RGp LB %RGp enmKind=%d\n", GCPhys, cb, enmKind));
    NOREF(pVM); NOREF(enmKind); NOREF(GCPhys); NOREF(cb);
}


void nemHCNativeNotifyHandlerPhysicalDeregister(PVM pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhys, RTGCPHYS cb,
                                                int fRestoreAsRAM, bool fRestoreAsRAM2)
{
    Log5(("nemHCNativeNotifyHandlerPhysicalDeregister: %RGp LB %RGp enmKind=%d fRestoreAsRAM=%d fRestoreAsRAM2=%d\n",
          GCPhys, cb, enmKind, fRestoreAsRAM, fRestoreAsRAM2));
    NOREF(pVM); NOREF(enmKind); NOREF(GCPhys); NOREF(cb); NOREF(fRestoreAsRAM); NOREF(fRestoreAsRAM2);
}


void nemHCNativeNotifyHandlerPhysicalModify(PVM pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhysOld,
                                            RTGCPHYS GCPhysNew, RTGCPHYS cb, bool fRestoreAsRAM)
{
    Log5(("nemHCNativeNotifyHandlerPhysicalModify: %RGp LB %RGp -> %RGp enmKind=%d fRestoreAsRAM=%d\n",
          GCPhysOld, cb, GCPhysNew, enmKind, fRestoreAsRAM));
    NOREF(pVM); NOREF(enmKind); NOREF(GCPhysOld); NOREF(GCPhysNew); NOREF(cb); NOREF(fRestoreAsRAM);
}


/**
 * Worker that maps pages into Hyper-V.
 *
 * This is used by the PGM physical page notifications as well as the memory
 * access VMEXIT handlers.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context virtual CPU structure of the
 *                          calling EMT.
 * @param   GCPhysSrc       The source page address.
 * @param   GCPhysDst       The hyper-V destination page.  This may differ from
 *                          GCPhysSrc when A20 is disabled.
 * @param   fPageProt       NEM_PAGE_PROT_XXX.
 * @param   pu2State        Our page state (input/output).
 * @param   fBackingChanged Set if the page backing is being changed.
 * @thread  EMT(pVCpu)
 */
NEM_TMPL_STATIC int nemHCNativeSetPhysPage(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhysSrc, RTGCPHYS GCPhysDst,
                                           uint32_t fPageProt, uint8_t *pu2State, bool fBackingChanged)
{
#ifdef NEM_WIN_USE_HYPERCALLS_FOR_PAGES
    /*
     * When using the hypercalls instead of the ring-3 APIs, we don't need to
     * unmap memory before modifying it.  We still want to track the state though,
     * since unmap will fail when called an unmapped page and we don't want to redo
     * upgrades/downgrades.
     */
    uint8_t const u2OldState = *pu2State;
    int rc;
    if (fPageProt == NEM_PAGE_PROT_NONE)
    {
        if (u2OldState > NEM_WIN_PAGE_STATE_UNMAPPED)
        {
            rc = nemHCWinHypercallUnmapPage(pVM, pVCpu, GCPhysDst);
            if (RT_SUCCESS(rc))
            {
                *pu2State = NEM_WIN_PAGE_STATE_UNMAPPED;
                uint32_t cMappedPages = ASMAtomicDecU32(&pVM->nem.s.cMappedPages); NOREF(cMappedPages);
                Log5(("NEM GPA unmapped/set: %RGp (was %s, cMappedPages=%u)\n", GCPhysDst, g_apszPageStates[u2OldState], cMappedPages));
            }
            else
                AssertLogRelMsgFailed(("nemHCNativeSetPhysPage/unmap: GCPhysDst=%RGp rc=%Rrc\n", GCPhysDst, rc));
        }
        else
            rc = VINF_SUCCESS;
    }
    else if (fPageProt & NEM_PAGE_PROT_WRITE)
    {
        if (u2OldState != NEM_WIN_PAGE_STATE_WRITABLE || fBackingChanged)
        {
            rc = nemHCWinHypercallMapPage(pVM, pVCpu, GCPhysSrc, GCPhysDst,
                                            HV_MAP_GPA_READABLE   | HV_MAP_GPA_WRITABLE
                                          | HV_MAP_GPA_EXECUTABLE | HV_MAP_GPA_EXECUTABLE_AGAIN);
            if (RT_SUCCESS(rc))
            {
                *pu2State = NEM_WIN_PAGE_STATE_WRITABLE;
                uint32_t cMappedPages = u2OldState <= NEM_WIN_PAGE_STATE_UNMAPPED
                                      ? ASMAtomicIncU32(&pVM->nem.s.cMappedPages) : pVM->nem.s.cMappedPages;
                Log5(("NEM GPA writable/set: %RGp (was %s, cMappedPages=%u)\n", GCPhysDst, g_apszPageStates[u2OldState], cMappedPages));
                NOREF(cMappedPages);
            }
            else
                AssertLogRelMsgFailed(("nemHCNativeSetPhysPage/writable: GCPhysDst=%RGp rc=%Rrc\n", GCPhysDst, rc));
        }
        else
            rc = VINF_SUCCESS;
    }
    else
    {
        if (u2OldState != NEM_WIN_PAGE_STATE_READABLE || fBackingChanged)
        {
            rc = nemHCWinHypercallMapPage(pVM, pVCpu, GCPhysSrc, GCPhysDst,
                                          HV_MAP_GPA_READABLE | HV_MAP_GPA_EXECUTABLE | HV_MAP_GPA_EXECUTABLE_AGAIN);
            if (RT_SUCCESS(rc))
            {
                *pu2State = NEM_WIN_PAGE_STATE_READABLE;
                uint32_t cMappedPages = u2OldState <= NEM_WIN_PAGE_STATE_UNMAPPED
                                      ? ASMAtomicIncU32(&pVM->nem.s.cMappedPages) : pVM->nem.s.cMappedPages;
                Log5(("NEM GPA read+exec/set: %RGp (was %s, cMappedPages=%u)\n", GCPhysDst, g_apszPageStates[u2OldState], cMappedPages));
                NOREF(cMappedPages);
            }
            else
                AssertLogRelMsgFailed(("nemHCNativeSetPhysPage/writable: GCPhysDst=%RGp rc=%Rrc\n", GCPhysDst, rc));
        }
        else
            rc = VINF_SUCCESS;
    }

    return VINF_SUCCESS;

#else
    /*
     * Looks like we need to unmap a page before we can change the backing
     * or even modify the protection.  This is going to be *REALLY* efficient.
     * PGM lends us two bits to keep track of the state here.
     */
    uint8_t const u2OldState = *pu2State;
    uint8_t const u2NewState = fPageProt & NEM_PAGE_PROT_WRITE ? NEM_WIN_PAGE_STATE_WRITABLE
                             : fPageProt & NEM_PAGE_PROT_READ  ? NEM_WIN_PAGE_STATE_READABLE : NEM_WIN_PAGE_STATE_UNMAPPED;
    if (   fBackingChanged
        || u2NewState != u2OldState)
    {
        if (u2OldState > NEM_WIN_PAGE_STATE_UNMAPPED)
        {
# ifdef NEM_WIN_USE_HYPERCALLS_FOR_PAGES
            int rc = nemHCWinHypercallUnmapPage(pVM, pVCpu, GCPhysDst);
            AssertRC(rc);
            if (RT_SUCCESS(rc))
            {
                *pu2State = NEM_WIN_PAGE_STATE_UNMAPPED;
                uint32_t cMappedPages = ASMAtomicDecU32(&pVM->nem.s.cMappedPages); NOREF(cMappedPages);
                if (u2NewState == NEM_WIN_PAGE_STATE_UNMAPPED)
                {
                    Log5(("NEM GPA unmapped/set: %RGp (was %s, cMappedPages=%u)\n",
                          GCPhysDst, g_apszPageStates[u2OldState], cMappedPages));
                    return VINF_SUCCESS;
                }
            }
            else
            {
                LogRel(("nemHCNativeSetPhysPage/unmap: GCPhysDst=%RGp rc=%Rrc\n", GCPhysDst, rc));
                return rc;
            }
# else
            HRESULT hrc = WHvUnmapGpaRange(pVM->nem.s.hPartition, GCPhysDst, X86_PAGE_SIZE);
            if (SUCCEEDED(hrc))
            {
                *pu2State = NEM_WIN_PAGE_STATE_UNMAPPED;
                uint32_t cMappedPages = ASMAtomicDecU32(&pVM->nem.s.cMappedPages); NOREF(cMappedPages);
                if (u2NewState == NEM_WIN_PAGE_STATE_UNMAPPED)
                {
                    Log5(("NEM GPA unmapped/set: %RGp (was %s, cMappedPages=%u)\n",
                          GCPhysDst, g_apszPageStates[u2OldState], cMappedPages));
                    return VINF_SUCCESS;
                }
            }
            else
            {
                LogRel(("nemHCNativeSetPhysPage/unmap: GCPhysDst=%RGp hrc=%Rhrc (%#x) Last=%#x/%u\n",
                        GCPhysDst, hrc, hrc, RTNtLastStatusValue(), RTNtLastErrorValue()));
                return VERR_NEM_INIT_FAILED;
            }
# endif
        }
    }

    /*
     * Writeable mapping?
     */
    if (fPageProt & NEM_PAGE_PROT_WRITE)
    {
# ifdef NEM_WIN_USE_HYPERCALLS_FOR_PAGES
        int rc = nemHCWinHypercallMapPage(pVM, pVCpu, GCPhysSrc, GCPhysDst,
                                            HV_MAP_GPA_READABLE   | HV_MAP_GPA_WRITABLE
                                          | HV_MAP_GPA_EXECUTABLE | HV_MAP_GPA_EXECUTABLE_AGAIN);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            *pu2State = NEM_WIN_PAGE_STATE_WRITABLE;
            uint32_t cMappedPages = ASMAtomicIncU32(&pVM->nem.s.cMappedPages); NOREF(cMappedPages);
            Log5(("NEM GPA mapped/set: %RGp %s (was %s, cMappedPages=%u)\n",
                  GCPhysDst, g_apszPageStates[u2NewState], g_apszPageStates[u2OldState], cMappedPages));
            return VINF_SUCCESS;
        }
        LogRel(("nemHCNativeSetPhysPage/writable: GCPhysDst=%RGp rc=%Rrc\n", GCPhysDst, rc));
        return rc;
# else
        void *pvPage;
        int rc = nemR3NativeGCPhys2R3PtrWriteable(pVM, GCPhysSrc, &pvPage);
        if (RT_SUCCESS(rc))
        {
            HRESULT hrc = WHvMapGpaRange(pVM->nem.s.hPartition, pvPage, GCPhysDst, X86_PAGE_SIZE,
                                         WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagExecute | WHvMapGpaRangeFlagWrite);
            if (SUCCEEDED(hrc))
            {
                *pu2State = NEM_WIN_PAGE_STATE_WRITABLE;
                uint32_t cMappedPages = ASMAtomicIncU32(&pVM->nem.s.cMappedPages); NOREF(cMappedPages);
                Log5(("NEM GPA mapped/set: %RGp %s (was %s, cMappedPages=%u)\n",
                      GCPhysDst, g_apszPageStates[u2NewState], g_apszPageStates[u2OldState], cMappedPages));
                return VINF_SUCCESS;
            }
            LogRel(("nemHCNativeSetPhysPage/writable: GCPhysDst=%RGp hrc=%Rhrc (%#x) Last=%#x/%u\n",
                    GCPhysDst, hrc, hrc, RTNtLastStatusValue(), RTNtLastErrorValue()));
            return VERR_NEM_INIT_FAILED;
        }
        LogRel(("nemHCNativeSetPhysPage/writable: GCPhysSrc=%RGp rc=%Rrc\n", GCPhysSrc, rc));
        return rc;
# endif
    }

    if (fPageProt & NEM_PAGE_PROT_READ)
    {
# ifdef NEM_WIN_USE_HYPERCALLS_FOR_PAGES
        int rc = nemHCWinHypercallMapPage(pVM, pVCpu, GCPhysSrc, GCPhysDst,
                                          HV_MAP_GPA_READABLE | HV_MAP_GPA_EXECUTABLE | HV_MAP_GPA_EXECUTABLE_AGAIN);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            *pu2State = NEM_WIN_PAGE_STATE_READABLE;
            uint32_t cMappedPages = ASMAtomicIncU32(&pVM->nem.s.cMappedPages); NOREF(cMappedPages);
            Log5(("NEM GPA mapped/set: %RGp %s (was %s, cMappedPages=%u)\n",
                  GCPhysDst, g_apszPageStates[u2NewState], g_apszPageStates[u2OldState], cMappedPages));
            return VINF_SUCCESS;
        }
        LogRel(("nemHCNativeSetPhysPage/readonly: GCPhysDst=%RGp rc=%Rrc\n", GCPhysDst, rc));
        return rc;
# else
        const void *pvPage;
        int rc = nemR3NativeGCPhys2R3PtrReadOnly(pVM, GCPhysSrc, &pvPage);
        if (RT_SUCCESS(rc))
        {
            HRESULT hrc = WHvMapGpaRange(pVM->nem.s.hPartition, (void *)pvPage, GCPhysDst, X86_PAGE_SIZE,
                                         WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagExecute);
            if (SUCCEEDED(hrc))
            {
                *pu2State = NEM_WIN_PAGE_STATE_READABLE;
                uint32_t cMappedPages = ASMAtomicIncU32(&pVM->nem.s.cMappedPages); NOREF(cMappedPages);
                Log5(("NEM GPA mapped/set: %RGp %s (was %s, cMappedPages=%u)\n",
                      GCPhysDst, g_apszPageStates[u2NewState], g_apszPageStates[u2OldState], cMappedPages));
                return VINF_SUCCESS;
            }
            LogRel(("nemHCNativeSetPhysPage/readonly: GCPhysDst=%RGp hrc=%Rhrc (%#x) Last=%#x/%u\n",
                    GCPhysDst, hrc, hrc, RTNtLastStatusValue(), RTNtLastErrorValue()));
            return VERR_NEM_INIT_FAILED;
        }
        LogRel(("nemHCNativeSetPhysPage/readonly: GCPhysSrc=%RGp rc=%Rrc\n", GCPhysSrc, rc));
        return rc;
# endif
    }

    /* We already unmapped it above. */
    *pu2State = NEM_WIN_PAGE_STATE_UNMAPPED;
    return VINF_SUCCESS;
#endif /* !NEM_WIN_USE_HYPERCALLS_FOR_PAGES */
}


NEM_TMPL_STATIC int nemHCJustUnmapPageFromHyperV(PVM pVM, RTGCPHYS GCPhysDst, uint8_t *pu2State)
{
    if (*pu2State <= NEM_WIN_PAGE_STATE_UNMAPPED)
    {
        Log5(("nemHCJustUnmapPageFromHyperV: %RGp == unmapped\n", GCPhysDst));
        *pu2State = NEM_WIN_PAGE_STATE_UNMAPPED;
        return VINF_SUCCESS;
    }

#if defined(NEM_WIN_USE_HYPERCALLS_FOR_PAGES) || defined(IN_RING0)
    PVMCPU pVCpu = VMMGetCpu(pVM);
    int rc = nemHCWinHypercallUnmapPage(pVM, pVCpu, GCPhysDst);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        uint32_t cMappedPages = ASMAtomicDecU32(&pVM->nem.s.cMappedPages); NOREF(cMappedPages);
        Log5(("NEM GPA unmapped/just: %RGp (was %s, cMappedPages=%u)\n", GCPhysDst, g_apszPageStates[*pu2State], cMappedPages));
        *pu2State = NEM_WIN_PAGE_STATE_UNMAPPED;
        return VINF_SUCCESS;
    }
    LogRel(("nemHCJustUnmapPageFromHyperV/unmap: GCPhysDst=%RGp rc=%Rrc\n", GCPhysDst, rc));
    return rc;
#else
    HRESULT hrc = WHvUnmapGpaRange(pVM->nem.s.hPartition, GCPhysDst & ~(RTGCPHYS)X86_PAGE_OFFSET_MASK, X86_PAGE_SIZE);
    if (SUCCEEDED(hrc))
    {
        uint32_t cMappedPages = ASMAtomicDecU32(&pVM->nem.s.cMappedPages); NOREF(cMappedPages);
        *pu2State = NEM_WIN_PAGE_STATE_UNMAPPED;
        Log5(("nemHCJustUnmapPageFromHyperV: %RGp => unmapped (total %u)\n", GCPhysDst, cMappedPages));
        return VINF_SUCCESS;
    }
    LogRel(("nemHCJustUnmapPageFromHyperV(%RGp): failed! hrc=%Rhrc (%#x) Last=%#x/%u\n",
            GCPhysDst, hrc, hrc, RTNtLastStatusValue(), RTNtLastErrorValue()));
    return VERR_INTERNAL_ERROR_3;
#endif
}


int nemHCNativeNotifyPhysPageAllocated(PVM pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhys, uint32_t fPageProt,
                                       PGMPAGETYPE enmType, uint8_t *pu2State)
{
    Log5(("nemHCNativeNotifyPhysPageAllocated: %RGp HCPhys=%RHp fPageProt=%#x enmType=%d *pu2State=%d\n",
          GCPhys, HCPhys, fPageProt, enmType, *pu2State));
    RT_NOREF_PV(HCPhys); RT_NOREF_PV(enmType);

    int rc;
#if defined(NEM_WIN_USE_HYPERCALLS_FOR_PAGES) || defined(IN_RING0)
    PVMCPU pVCpu = VMMGetCpu(pVM);
    if (   pVM->nem.s.fA20Enabled
        || !NEM_WIN_IS_RELEVANT_TO_A20(GCPhys))
        rc = nemHCNativeSetPhysPage(pVM, pVCpu, GCPhys, GCPhys, fPageProt, pu2State, true /*fBackingChanged*/);
    else
    {
        /* To keep effort at a minimum, we unmap the HMA page alias and resync it lazily when needed. */
        rc = nemHCWinUnmapPageForA20Gate(pVM, pVCpu, GCPhys | RT_BIT_32(20));
        if (!NEM_WIN_IS_SUBJECT_TO_A20(GCPhys) && RT_SUCCESS(rc))
            rc = nemHCNativeSetPhysPage(pVM, pVCpu, GCPhys, GCPhys, fPageProt, pu2State, true /*fBackingChanged*/);

    }
#else
    RT_NOREF_PV(fPageProt);
    if (   pVM->nem.s.fA20Enabled
        || !NEM_WIN_IS_RELEVANT_TO_A20(GCPhys))
        rc = nemR3JustUnmapPageFromHyperV(pVM, GCPhys, pu2State);
    else if (!NEM_WIN_IS_SUBJECT_TO_A20(GCPhys))
        rc = nemR3JustUnmapPageFromHyperV(pVM, GCPhys, pu2State);
    else
        rc = VINF_SUCCESS; /* ignore since we've got the alias page at this address. */
#endif
    return rc;
}


void nemHCNativeNotifyPhysPageProtChanged(PVM pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhys, uint32_t fPageProt,
                                          PGMPAGETYPE enmType, uint8_t *pu2State)
{
    Log5(("nemHCNativeNotifyPhysPageProtChanged: %RGp HCPhys=%RHp fPageProt=%#x enmType=%d *pu2State=%d\n",
          GCPhys, HCPhys, fPageProt, enmType, *pu2State));
    RT_NOREF_PV(HCPhys); RT_NOREF_PV(enmType);

#if defined(NEM_WIN_USE_HYPERCALLS_FOR_PAGES) || defined(IN_RING0)
    PVMCPU pVCpu = VMMGetCpu(pVM);
    if (   pVM->nem.s.fA20Enabled
        || !NEM_WIN_IS_RELEVANT_TO_A20(GCPhys))
        nemHCNativeSetPhysPage(pVM, pVCpu, GCPhys, GCPhys, fPageProt, pu2State, false /*fBackingChanged*/);
    else
    {
        /* To keep effort at a minimum, we unmap the HMA page alias and resync it lazily when needed. */
        nemHCWinUnmapPageForA20Gate(pVM, pVCpu, GCPhys | RT_BIT_32(20));
        if (!NEM_WIN_IS_SUBJECT_TO_A20(GCPhys))
            nemHCNativeSetPhysPage(pVM, pVCpu, GCPhys, GCPhys, fPageProt, pu2State, false /*fBackingChanged*/);
    }
#else
    RT_NOREF_PV(fPageProt);
    if (   pVM->nem.s.fA20Enabled
        || !NEM_WIN_IS_RELEVANT_TO_A20(GCPhys))
        nemR3JustUnmapPageFromHyperV(pVM, GCPhys, pu2State);
    else if (!NEM_WIN_IS_SUBJECT_TO_A20(GCPhys))
        nemR3JustUnmapPageFromHyperV(pVM, GCPhys, pu2State);
    /* else: ignore since we've got the alias page at this address. */
#endif
}


void nemHCNativeNotifyPhysPageChanged(PVM pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhysPrev, RTHCPHYS HCPhysNew,
                                     uint32_t fPageProt, PGMPAGETYPE enmType, uint8_t *pu2State)
{
    Log5(("nemHCNativeNotifyPhysPageChanged: %RGp HCPhys=%RHp->%RHp fPageProt=%#x enmType=%d *pu2State=%d\n",
          GCPhys, HCPhysPrev, HCPhysNew, fPageProt, enmType, *pu2State));
    RT_NOREF_PV(HCPhysPrev); RT_NOREF_PV(HCPhysNew); RT_NOREF_PV(enmType);

#if defined(NEM_WIN_USE_HYPERCALLS_FOR_PAGES) || defined(IN_RING0)
    PVMCPU pVCpu = VMMGetCpu(pVM);
    if (   pVM->nem.s.fA20Enabled
        || !NEM_WIN_IS_RELEVANT_TO_A20(GCPhys))
        nemHCNativeSetPhysPage(pVM, pVCpu, GCPhys, GCPhys, fPageProt, pu2State, true /*fBackingChanged*/);
    else
    {
        /* To keep effort at a minimum, we unmap the HMA page alias and resync it lazily when needed. */
        nemHCWinUnmapPageForA20Gate(pVM, pVCpu, GCPhys | RT_BIT_32(20));
        if (!NEM_WIN_IS_SUBJECT_TO_A20(GCPhys))
            nemHCNativeSetPhysPage(pVM, pVCpu, GCPhys, GCPhys, fPageProt, pu2State, true /*fBackingChanged*/);
    }
#else
    RT_NOREF_PV(fPageProt);
    if (   pVM->nem.s.fA20Enabled
        || !NEM_WIN_IS_RELEVANT_TO_A20(GCPhys))
        nemR3JustUnmapPageFromHyperV(pVM, GCPhys, pu2State);
    else if (!NEM_WIN_IS_SUBJECT_TO_A20(GCPhys))
        nemR3JustUnmapPageFromHyperV(pVM, GCPhys, pu2State);
    /* else: ignore since we've got the alias page at this address. */
#endif
}

