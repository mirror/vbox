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
    /*
     * The following is very similar to what nemR0WinExportState() does.
     */
    WHV_REGISTER_NAME  aenmNames[128];
    WHV_REGISTER_VALUE aValues[128];

    uint64_t const fWhat = ~pCtx->fExtrn & (CPUMCTX_EXTRN_ALL | CPUMCTX_EXTRN_NEM_WIN_MASK);
    if (   !fWhat
        && pVCpu->nem.s.fCurrentInterruptWindows == pVCpu->nem.s.fDesiredInterruptWindows)
        return VINF_SUCCESS;
    uintptr_t iReg = 0;

#  define ADD_REG64(a_enmName, a_uValue) do { \
            aenmNames[iReg]      = (a_enmName); \
            aValues[iReg].Reg128.High64 = 0; \
            aValues[iReg].Reg64  = (a_uValue); \
            iReg++; \
        } while (0)
#  define ADD_REG128(a_enmName, a_uValueLo, a_uValueHi) do { \
            aenmNames[iReg] = (a_enmName); \
            aValues[iReg].Reg128.Low64  = (a_uValueLo); \
            aValues[iReg].Reg128.High64 = (a_uValueHi); \
            iReg++; \
        } while (0)

    /* GPRs */
    if (fWhat & CPUMCTX_EXTRN_GPRS_MASK)
    {
        if (fWhat & CPUMCTX_EXTRN_RAX)
            ADD_REG64(WHvX64RegisterRax, pCtx->rax);
        if (fWhat & CPUMCTX_EXTRN_RCX)
            ADD_REG64(WHvX64RegisterRcx, pCtx->rcx);
        if (fWhat & CPUMCTX_EXTRN_RDX)
            ADD_REG64(WHvX64RegisterRdx, pCtx->rdx);
        if (fWhat & CPUMCTX_EXTRN_RBX)
            ADD_REG64(WHvX64RegisterRbx, pCtx->rbx);
        if (fWhat & CPUMCTX_EXTRN_RSP)
            ADD_REG64(WHvX64RegisterRsp, pCtx->rsp);
        if (fWhat & CPUMCTX_EXTRN_RBP)
            ADD_REG64(WHvX64RegisterRbp, pCtx->rbp);
        if (fWhat & CPUMCTX_EXTRN_RSI)
            ADD_REG64(WHvX64RegisterRsi, pCtx->rsi);
        if (fWhat & CPUMCTX_EXTRN_RDI)
            ADD_REG64(WHvX64RegisterRdi, pCtx->rdi);
        if (fWhat & CPUMCTX_EXTRN_R8_R15)
        {
            ADD_REG64(WHvX64RegisterR8, pCtx->r8);
            ADD_REG64(WHvX64RegisterR9, pCtx->r9);
            ADD_REG64(WHvX64RegisterR10, pCtx->r10);
            ADD_REG64(WHvX64RegisterR11, pCtx->r11);
            ADD_REG64(WHvX64RegisterR12, pCtx->r12);
            ADD_REG64(WHvX64RegisterR13, pCtx->r13);
            ADD_REG64(WHvX64RegisterR14, pCtx->r14);
            ADD_REG64(WHvX64RegisterR15, pCtx->r15);
        }
    }

    /* RIP & Flags */
    if (fWhat & CPUMCTX_EXTRN_RIP)
        ADD_REG64(WHvX64RegisterRip, pCtx->rip);
    if (fWhat & CPUMCTX_EXTRN_RFLAGS)
        ADD_REG64(WHvX64RegisterRflags, pCtx->rflags.u);

    /* Segments */
#  define ADD_SEG(a_enmName, a_SReg) \
        do { \
            aenmNames[iReg]                  = a_enmName; \
            aValues[iReg].Segment.Base       = (a_SReg).u64Base; \
            aValues[iReg].Segment.Limit      = (a_SReg).u32Limit; \
            aValues[iReg].Segment.Selector   = (a_SReg).Sel; \
            aValues[iReg].Segment.Attributes = (a_SReg).Attr.u; \
            iReg++; \
        } while (0)
    if (fWhat & CPUMCTX_EXTRN_SREG_MASK)
    {
        if (fWhat & CPUMCTX_EXTRN_ES)
            ADD_SEG(WHvX64RegisterEs,   pCtx->es);
        if (fWhat & CPUMCTX_EXTRN_CS)
            ADD_SEG(WHvX64RegisterCs,   pCtx->cs);
        if (fWhat & CPUMCTX_EXTRN_SS)
            ADD_SEG(WHvX64RegisterSs,   pCtx->ss);
        if (fWhat & CPUMCTX_EXTRN_DS)
            ADD_SEG(WHvX64RegisterDs,   pCtx->ds);
        if (fWhat & CPUMCTX_EXTRN_FS)
            ADD_SEG(WHvX64RegisterFs,   pCtx->fs);
        if (fWhat & CPUMCTX_EXTRN_GS)
            ADD_SEG(WHvX64RegisterGs,   pCtx->gs);
    }

    /* Descriptor tables & task segment. */
    if (fWhat & CPUMCTX_EXTRN_TABLE_MASK)
    {
        if (fWhat & CPUMCTX_EXTRN_LDTR)
            ADD_SEG(WHvX64RegisterLdtr, pCtx->ldtr);
        if (fWhat & CPUMCTX_EXTRN_TR)
            ADD_SEG(WHvX64RegisterTr,   pCtx->tr);
        if (fWhat & CPUMCTX_EXTRN_IDTR)
        {
            aenmNames[iReg] = WHvX64RegisterIdtr;
            aValues[iReg].Table.Limit = pCtx->idtr.cbIdt;
            aValues[iReg].Table.Base  = pCtx->idtr.pIdt;
            iReg++;
        }
        if (fWhat & CPUMCTX_EXTRN_GDTR)
        {
            aenmNames[iReg] = WHvX64RegisterGdtr;
            aValues[iReg].Table.Limit = pCtx->gdtr.cbGdt;
            aValues[iReg].Table.Base  = pCtx->gdtr.pGdt;
            iReg++;
        }
    }

    /* Control registers. */
    if (fWhat & CPUMCTX_EXTRN_CR_MASK)
    {
        if (fWhat & CPUMCTX_EXTRN_CR0)
            ADD_REG64(WHvX64RegisterCr0, pCtx->cr0);
        if (fWhat & CPUMCTX_EXTRN_CR2)
            ADD_REG64(WHvX64RegisterCr2, pCtx->cr2);
        if (fWhat & CPUMCTX_EXTRN_CR3)
            ADD_REG64(WHvX64RegisterCr3, pCtx->cr3);
        if (fWhat & CPUMCTX_EXTRN_CR4)
            ADD_REG64(WHvX64RegisterCr4, pCtx->cr4);
    }

    /** @todo CR8/TPR */
    ADD_REG64(WHvX64RegisterCr8, CPUMGetGuestCR8(pVCpu));

    /* Debug registers. */
/** @todo fixme. Figure out what the hyper-v version of KVM_SET_GUEST_DEBUG would be. */
    if (fWhat & CPUMCTX_EXTRN_DR0_DR3)
    {
        ADD_REG64(WHvX64RegisterDr0, pCtx->dr[0]); // CPUMGetHyperDR0(pVCpu));
        ADD_REG64(WHvX64RegisterDr1, pCtx->dr[1]); // CPUMGetHyperDR1(pVCpu));
        ADD_REG64(WHvX64RegisterDr2, pCtx->dr[2]); // CPUMGetHyperDR2(pVCpu));
        ADD_REG64(WHvX64RegisterDr3, pCtx->dr[3]); // CPUMGetHyperDR3(pVCpu));
    }
    if (fWhat & CPUMCTX_EXTRN_DR6)
        ADD_REG64(WHvX64RegisterDr6, pCtx->dr[6]); // CPUMGetHyperDR6(pVCpu));
    if (fWhat & CPUMCTX_EXTRN_DR7)
        ADD_REG64(WHvX64RegisterDr7, pCtx->dr[7]); // CPUMGetHyperDR7(pVCpu));

    /* Floating point state. */
    if (fWhat & CPUMCTX_EXTRN_X87)
    {
        ADD_REG128(WHvX64RegisterFpMmx0, pCtx->pXStateR3->x87.aRegs[0].au64[0], pCtx->pXStateR3->x87.aRegs[0].au64[1]);
        ADD_REG128(WHvX64RegisterFpMmx1, pCtx->pXStateR3->x87.aRegs[1].au64[0], pCtx->pXStateR3->x87.aRegs[1].au64[1]);
        ADD_REG128(WHvX64RegisterFpMmx2, pCtx->pXStateR3->x87.aRegs[2].au64[0], pCtx->pXStateR3->x87.aRegs[2].au64[1]);
        ADD_REG128(WHvX64RegisterFpMmx3, pCtx->pXStateR3->x87.aRegs[3].au64[0], pCtx->pXStateR3->x87.aRegs[3].au64[1]);
        ADD_REG128(WHvX64RegisterFpMmx4, pCtx->pXStateR3->x87.aRegs[4].au64[0], pCtx->pXStateR3->x87.aRegs[4].au64[1]);
        ADD_REG128(WHvX64RegisterFpMmx5, pCtx->pXStateR3->x87.aRegs[5].au64[0], pCtx->pXStateR3->x87.aRegs[5].au64[1]);
        ADD_REG128(WHvX64RegisterFpMmx6, pCtx->pXStateR3->x87.aRegs[6].au64[0], pCtx->pXStateR3->x87.aRegs[6].au64[1]);
        ADD_REG128(WHvX64RegisterFpMmx7, pCtx->pXStateR3->x87.aRegs[7].au64[0], pCtx->pXStateR3->x87.aRegs[7].au64[1]);

        aenmNames[iReg] = WHvX64RegisterFpControlStatus;
        aValues[iReg].FpControlStatus.FpControl = pCtx->pXStateR3->x87.FCW;
        aValues[iReg].FpControlStatus.FpStatus  = pCtx->pXStateR3->x87.FSW;
        aValues[iReg].FpControlStatus.FpTag     = pCtx->pXStateR3->x87.FTW;
        aValues[iReg].FpControlStatus.Reserved  = pCtx->pXStateR3->x87.FTW >> 8;
        aValues[iReg].FpControlStatus.LastFpOp  = pCtx->pXStateR3->x87.FOP;
        aValues[iReg].FpControlStatus.LastFpRip = (pCtx->pXStateR3->x87.FPUIP)
                                                | ((uint64_t)pCtx->pXStateR3->x87.CS << 32)
                                                | ((uint64_t)pCtx->pXStateR3->x87.Rsrvd1 << 48);
        iReg++;

        aenmNames[iReg] = WHvX64RegisterXmmControlStatus;
        aValues[iReg].XmmControlStatus.LastFpRdp            = (pCtx->pXStateR3->x87.FPUDP)
                                                            | ((uint64_t)pCtx->pXStateR3->x87.DS << 32)
                                                            | ((uint64_t)pCtx->pXStateR3->x87.Rsrvd2 << 48);
        aValues[iReg].XmmControlStatus.XmmStatusControl     = pCtx->pXStateR3->x87.MXCSR;
        aValues[iReg].XmmControlStatus.XmmStatusControlMask = pCtx->pXStateR3->x87.MXCSR_MASK; /** @todo ??? (Isn't this an output field?) */
        iReg++;
    }

    /* Vector state. */
    if (fWhat & CPUMCTX_EXTRN_SSE_AVX)
    {
        ADD_REG128(WHvX64RegisterXmm0,  pCtx->pXStateR3->x87.aXMM[ 0].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[ 0].uXmm.s.Hi);
        ADD_REG128(WHvX64RegisterXmm1,  pCtx->pXStateR3->x87.aXMM[ 1].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[ 1].uXmm.s.Hi);
        ADD_REG128(WHvX64RegisterXmm2,  pCtx->pXStateR3->x87.aXMM[ 2].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[ 2].uXmm.s.Hi);
        ADD_REG128(WHvX64RegisterXmm3,  pCtx->pXStateR3->x87.aXMM[ 3].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[ 3].uXmm.s.Hi);
        ADD_REG128(WHvX64RegisterXmm4,  pCtx->pXStateR3->x87.aXMM[ 4].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[ 4].uXmm.s.Hi);
        ADD_REG128(WHvX64RegisterXmm5,  pCtx->pXStateR3->x87.aXMM[ 5].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[ 5].uXmm.s.Hi);
        ADD_REG128(WHvX64RegisterXmm6,  pCtx->pXStateR3->x87.aXMM[ 6].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[ 6].uXmm.s.Hi);
        ADD_REG128(WHvX64RegisterXmm7,  pCtx->pXStateR3->x87.aXMM[ 7].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[ 7].uXmm.s.Hi);
        ADD_REG128(WHvX64RegisterXmm8,  pCtx->pXStateR3->x87.aXMM[ 8].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[ 8].uXmm.s.Hi);
        ADD_REG128(WHvX64RegisterXmm9,  pCtx->pXStateR3->x87.aXMM[ 9].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[ 9].uXmm.s.Hi);
        ADD_REG128(WHvX64RegisterXmm10, pCtx->pXStateR3->x87.aXMM[10].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[10].uXmm.s.Hi);
        ADD_REG128(WHvX64RegisterXmm10, pCtx->pXStateR3->x87.aXMM[11].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[11].uXmm.s.Hi);
        ADD_REG128(WHvX64RegisterXmm10, pCtx->pXStateR3->x87.aXMM[12].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[12].uXmm.s.Hi);
        ADD_REG128(WHvX64RegisterXmm10, pCtx->pXStateR3->x87.aXMM[13].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[13].uXmm.s.Hi);
        ADD_REG128(WHvX64RegisterXmm10, pCtx->pXStateR3->x87.aXMM[14].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[14].uXmm.s.Hi);
        ADD_REG128(WHvX64RegisterXmm10, pCtx->pXStateR3->x87.aXMM[15].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[15].uXmm.s.Hi);
    }

    /* MSRs */
    // WHvX64RegisterTsc - don't touch
    if (fWhat & CPUMCTX_EXTRN_EFER)
        ADD_REG64(WHvX64RegisterEfer, pCtx->msrEFER);
    if (fWhat & CPUMCTX_EXTRN_KERNEL_GS_BASE)
        ADD_REG64(WHvX64RegisterKernelGsBase, pCtx->msrKERNELGSBASE);
    if (fWhat & CPUMCTX_EXTRN_SYSENTER_MSRS)
    {
        ADD_REG64(WHvX64RegisterSysenterCs, pCtx->SysEnter.cs);
        ADD_REG64(WHvX64RegisterSysenterEip, pCtx->SysEnter.eip);
        ADD_REG64(WHvX64RegisterSysenterEsp, pCtx->SysEnter.esp);
    }
    if (fWhat & CPUMCTX_EXTRN_SYSCALL_MSRS)
    {
        ADD_REG64(WHvX64RegisterStar, pCtx->msrSTAR);
        ADD_REG64(WHvX64RegisterLstar, pCtx->msrLSTAR);
        ADD_REG64(WHvX64RegisterCstar, pCtx->msrCSTAR);
        ADD_REG64(WHvX64RegisterSfmask, pCtx->msrSFMASK);
    }
    if (fWhat & CPUMCTX_EXTRN_OTHER_MSRS)
    {
        ADD_REG64(WHvX64RegisterApicBase, APICGetBaseMsrNoCheck(pVCpu));
        ADD_REG64(WHvX64RegisterPat, pCtx->msrPAT);
#if 0 /** @todo check if WHvX64RegisterMsrMtrrCap works here... */
        ADD_REG64(WHvX64RegisterMsrMtrrCap, CPUMGetGuestIa32MtrrCap(pVCpu));
#endif
        PCPUMCTXMSRS pCtxMsrs = CPUMQueryGuestCtxMsrsPtr(pVCpu);
        ADD_REG64(WHvX64RegisterMsrMtrrDefType, pCtxMsrs->msr.MtrrDefType);
        ADD_REG64(WHvX64RegisterMsrMtrrFix64k00000, pCtxMsrs->msr.MtrrFix64K_00000);
        ADD_REG64(WHvX64RegisterMsrMtrrFix16k80000, pCtxMsrs->msr.MtrrFix16K_80000);
        ADD_REG64(WHvX64RegisterMsrMtrrFix16kA0000, pCtxMsrs->msr.MtrrFix16K_A0000);
        ADD_REG64(WHvX64RegisterMsrMtrrFix4kC0000,  pCtxMsrs->msr.MtrrFix4K_C0000);
        ADD_REG64(WHvX64RegisterMsrMtrrFix4kC8000,  pCtxMsrs->msr.MtrrFix4K_C8000);
        ADD_REG64(WHvX64RegisterMsrMtrrFix4kD0000,  pCtxMsrs->msr.MtrrFix4K_D0000);
        ADD_REG64(WHvX64RegisterMsrMtrrFix4kD8000,  pCtxMsrs->msr.MtrrFix4K_D8000);
        ADD_REG64(WHvX64RegisterMsrMtrrFix4kE0000,  pCtxMsrs->msr.MtrrFix4K_E0000);
        ADD_REG64(WHvX64RegisterMsrMtrrFix4kE8000,  pCtxMsrs->msr.MtrrFix4K_E8000);
        ADD_REG64(WHvX64RegisterMsrMtrrFix4kF0000,  pCtxMsrs->msr.MtrrFix4K_F0000);
        ADD_REG64(WHvX64RegisterMsrMtrrFix4kF8000,  pCtxMsrs->msr.MtrrFix4K_F8000);
        ADD_REG64(WHvX64RegisterTscAux, pCtxMsrs->msr.TscAux);
#if 0 /** @todo these registers aren't available? Might explain something.. .*/
        const CPUMCPUVENDOR enmCpuVendor = CPUMGetHostCpuVendor(pGVM->pVM);
        if (enmCpuVendor != CPUMCPUVENDOR_AMD)
        {
            ADD_REG64(HvX64RegisterIa32MiscEnable, pCtxMsrs->msr.MiscEnable);
            ADD_REG64(HvX64RegisterIa32FeatureControl, CPUMGetGuestIa32FeatureControl(pVCpu));
        }
#endif
    }

    /* event injection (clear it). */
    if (fWhat & CPUMCTX_EXTRN_NEM_WIN_EVENT_INJECT)
        ADD_REG64(WHvRegisterPendingInterruption, 0);

    /* Interruptibility state.  This can get a little complicated since we get
       half of the state via HV_X64_VP_EXECUTION_STATE. */
    if (   (fWhat & (CPUMCTX_EXTRN_NEM_WIN_INHIBIT_INT | CPUMCTX_EXTRN_NEM_WIN_INHIBIT_NMI))
        ==          (CPUMCTX_EXTRN_NEM_WIN_INHIBIT_INT | CPUMCTX_EXTRN_NEM_WIN_INHIBIT_NMI) )
    {
        ADD_REG64(WHvRegisterInterruptState, 0);
        if (   VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS)
            && EMGetInhibitInterruptsPC(pVCpu) == pCtx->rip)
            aValues[iReg - 1].InterruptState.InterruptShadow = 1;
        if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_BLOCK_NMIS))
            aValues[iReg - 1].InterruptState.NmiMasked = 1;
    }
    else if (fWhat & CPUMCTX_EXTRN_NEM_WIN_INHIBIT_INT)
    {
        if (   pVCpu->nem.s.fLastInterruptShadow
            || (   VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS)
                && EMGetInhibitInterruptsPC(pVCpu) == pCtx->rip))
        {
            ADD_REG64(WHvRegisterInterruptState, 0);
            if (   VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS)
                && EMGetInhibitInterruptsPC(pVCpu) == pCtx->rip)
                aValues[iReg - 1].InterruptState.InterruptShadow = 1;
            /** @todo Retrieve NMI state, currently assuming it's zero. (yes this may happen on I/O) */
            //if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_BLOCK_NMIS))
            //    aValues[iReg - 1].InterruptState.NmiMasked = 1;
        }
    }
    else
        Assert(!(fWhat & CPUMCTX_EXTRN_NEM_WIN_INHIBIT_NMI));

    /* Interrupt windows. Always set if active as Hyper-V seems to be forgetful. */
    uint8_t const fDesiredIntWin = pVCpu->nem.s.fDesiredInterruptWindows;
    if (   fDesiredIntWin
        || pVCpu->nem.s.fCurrentInterruptWindows != fDesiredIntWin)
    {
        pVCpu->nem.s.fCurrentInterruptWindows = pVCpu->nem.s.fDesiredInterruptWindows;
        ADD_REG64(WHvX64RegisterDeliverabilityNotifications, fDesiredIntWin);
        Assert(aValues[iReg - 1].DeliverabilityNotifications.NmiNotification == RT_BOOL(fDesiredIntWin & NEM_WIN_INTW_F_NMI));
        Assert(aValues[iReg - 1].DeliverabilityNotifications.InterruptNotification == RT_BOOL(fDesiredIntWin & NEM_WIN_INTW_F_REGULAR));
        Assert(aValues[iReg - 1].DeliverabilityNotifications.InterruptPriority == (fDesiredIntWin & NEM_WIN_INTW_F_PRIO_MASK) >> NEM_WIN_INTW_F_PRIO_SHIFT);
    }

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
    {
        pCtx->fExtrn |= CPUMCTX_EXTRN_ALL | CPUMCTX_EXTRN_NEM_WIN_MASK | CPUMCTX_EXTRN_KEEPER_NEM;
        return VINF_SUCCESS;
    }
    AssertLogRelMsgFailed(("WHvSetVirtualProcessorRegisters(%p, %u,,%u,) -> %Rhrc (Last=%#x/%u)\n",
                           pVM->nem.s.hPartition, pVCpu->idCpu, iReg,
                           hrc, RTNtLastStatusValue(), RTNtLastErrorValue()));
    return VERR_INTERNAL_ERROR;

#  undef ADD_REG64
#  undef ADD_REG128
#  undef ADD_SEG

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

    fWhat &= pCtx->fExtrn;
    uintptr_t iReg = 0;

    /* GPRs */
    if (fWhat & CPUMCTX_EXTRN_GPRS_MASK)
    {
        if (fWhat & CPUMCTX_EXTRN_RAX)
            aenmNames[iReg++] = WHvX64RegisterRax;
        if (fWhat & CPUMCTX_EXTRN_RCX)
            aenmNames[iReg++] = WHvX64RegisterRcx;
        if (fWhat & CPUMCTX_EXTRN_RDX)
            aenmNames[iReg++] = WHvX64RegisterRdx;
        if (fWhat & CPUMCTX_EXTRN_RBX)
            aenmNames[iReg++] = WHvX64RegisterRbx;
        if (fWhat & CPUMCTX_EXTRN_RSP)
            aenmNames[iReg++] = WHvX64RegisterRsp;
        if (fWhat & CPUMCTX_EXTRN_RBP)
            aenmNames[iReg++] = WHvX64RegisterRbp;
        if (fWhat & CPUMCTX_EXTRN_RSI)
            aenmNames[iReg++] = WHvX64RegisterRsi;
        if (fWhat & CPUMCTX_EXTRN_RDI)
            aenmNames[iReg++] = WHvX64RegisterRdi;
        if (fWhat & CPUMCTX_EXTRN_R8_R15)
        {
            aenmNames[iReg++] = WHvX64RegisterR8;
            aenmNames[iReg++] = WHvX64RegisterR9;
            aenmNames[iReg++] = WHvX64RegisterR10;
            aenmNames[iReg++] = WHvX64RegisterR11;
            aenmNames[iReg++] = WHvX64RegisterR12;
            aenmNames[iReg++] = WHvX64RegisterR13;
            aenmNames[iReg++] = WHvX64RegisterR14;
            aenmNames[iReg++] = WHvX64RegisterR15;
        }
    }

    /* RIP & Flags */
    if (fWhat & CPUMCTX_EXTRN_RIP)
        aenmNames[iReg++] = WHvX64RegisterRip;
    if (fWhat & CPUMCTX_EXTRN_RFLAGS)
        aenmNames[iReg++] = WHvX64RegisterRflags;

    /* Segments */
    if (fWhat & CPUMCTX_EXTRN_SREG_MASK)
    {
        if (fWhat & CPUMCTX_EXTRN_ES)
            aenmNames[iReg++] = WHvX64RegisterEs;
        if (fWhat & CPUMCTX_EXTRN_CS)
            aenmNames[iReg++] = WHvX64RegisterCs;
        if (fWhat & CPUMCTX_EXTRN_SS)
            aenmNames[iReg++] = WHvX64RegisterSs;
        if (fWhat & CPUMCTX_EXTRN_DS)
            aenmNames[iReg++] = WHvX64RegisterDs;
        if (fWhat & CPUMCTX_EXTRN_FS)
            aenmNames[iReg++] = WHvX64RegisterFs;
        if (fWhat & CPUMCTX_EXTRN_GS)
            aenmNames[iReg++] = WHvX64RegisterGs;
    }

    /* Descriptor tables. */
    if (fWhat & CPUMCTX_EXTRN_TABLE_MASK)
    {
        if (fWhat & CPUMCTX_EXTRN_LDTR)
            aenmNames[iReg++] = WHvX64RegisterLdtr;
        if (fWhat & CPUMCTX_EXTRN_TR)
            aenmNames[iReg++] = WHvX64RegisterTr;
        if (fWhat & CPUMCTX_EXTRN_IDTR)
            aenmNames[iReg++] = WHvX64RegisterIdtr;
        if (fWhat & CPUMCTX_EXTRN_GDTR)
            aenmNames[iReg++] = WHvX64RegisterGdtr;
    }

    /* Control registers. */
    if (fWhat & CPUMCTX_EXTRN_CR_MASK)
    {
        if (fWhat & CPUMCTX_EXTRN_CR0)
            aenmNames[iReg++] = WHvX64RegisterCr0;
        if (fWhat & CPUMCTX_EXTRN_CR2)
            aenmNames[iReg++] = WHvX64RegisterCr2;
        if (fWhat & CPUMCTX_EXTRN_CR3)
            aenmNames[iReg++] = WHvX64RegisterCr3;
        if (fWhat & CPUMCTX_EXTRN_CR4)
            aenmNames[iReg++] = WHvX64RegisterCr4;
    }
    aenmNames[iReg++] = WHvX64RegisterCr8; /// @todo CR8/TPR

    /* Debug registers. */
    if (fWhat & CPUMCTX_EXTRN_DR0_DR3)
    {
        aenmNames[iReg++] = WHvX64RegisterDr0;
        aenmNames[iReg++] = WHvX64RegisterDr1;
        aenmNames[iReg++] = WHvX64RegisterDr2;
        aenmNames[iReg++] = WHvX64RegisterDr3;
    }
    if (fWhat & CPUMCTX_EXTRN_DR6)
        aenmNames[iReg++] = WHvX64RegisterDr6;
    if (fWhat & CPUMCTX_EXTRN_DR7)
        aenmNames[iReg++] = WHvX64RegisterDr7;

    /* Floating point state. */
    if (fWhat & CPUMCTX_EXTRN_X87)
    {
        aenmNames[iReg++] = WHvX64RegisterFpMmx0;
        aenmNames[iReg++] = WHvX64RegisterFpMmx1;
        aenmNames[iReg++] = WHvX64RegisterFpMmx2;
        aenmNames[iReg++] = WHvX64RegisterFpMmx3;
        aenmNames[iReg++] = WHvX64RegisterFpMmx4;
        aenmNames[iReg++] = WHvX64RegisterFpMmx5;
        aenmNames[iReg++] = WHvX64RegisterFpMmx6;
        aenmNames[iReg++] = WHvX64RegisterFpMmx7;
        aenmNames[iReg++] = WHvX64RegisterFpControlStatus;
    }
    if (fWhat & (CPUMCTX_EXTRN_X87 | CPUMCTX_EXTRN_SSE_AVX))
        aenmNames[iReg++] = WHvX64RegisterXmmControlStatus;

    /* Vector state. */
    if (fWhat & CPUMCTX_EXTRN_SSE_AVX)
    {
        aenmNames[iReg++] = WHvX64RegisterXmm0;
        aenmNames[iReg++] = WHvX64RegisterXmm1;
        aenmNames[iReg++] = WHvX64RegisterXmm2;
        aenmNames[iReg++] = WHvX64RegisterXmm3;
        aenmNames[iReg++] = WHvX64RegisterXmm4;
        aenmNames[iReg++] = WHvX64RegisterXmm5;
        aenmNames[iReg++] = WHvX64RegisterXmm6;
        aenmNames[iReg++] = WHvX64RegisterXmm7;
        aenmNames[iReg++] = WHvX64RegisterXmm8;
        aenmNames[iReg++] = WHvX64RegisterXmm9;
        aenmNames[iReg++] = WHvX64RegisterXmm10;
        aenmNames[iReg++] = WHvX64RegisterXmm11;
        aenmNames[iReg++] = WHvX64RegisterXmm12;
        aenmNames[iReg++] = WHvX64RegisterXmm13;
        aenmNames[iReg++] = WHvX64RegisterXmm14;
        aenmNames[iReg++] = WHvX64RegisterXmm15;
    }

    /* MSRs */
    // WHvX64RegisterTsc - don't touch
    if (fWhat & CPUMCTX_EXTRN_EFER)
        aenmNames[iReg++] = WHvX64RegisterEfer;
    if (fWhat & CPUMCTX_EXTRN_KERNEL_GS_BASE)
        aenmNames[iReg++] = WHvX64RegisterKernelGsBase;
    if (fWhat & CPUMCTX_EXTRN_SYSENTER_MSRS)
    {
        aenmNames[iReg++] = WHvX64RegisterSysenterCs;
        aenmNames[iReg++] = WHvX64RegisterSysenterEip;
        aenmNames[iReg++] = WHvX64RegisterSysenterEsp;
    }
    if (fWhat & CPUMCTX_EXTRN_SYSCALL_MSRS)
    {
        aenmNames[iReg++] = WHvX64RegisterStar;
        aenmNames[iReg++] = WHvX64RegisterLstar;
        aenmNames[iReg++] = WHvX64RegisterCstar;
        aenmNames[iReg++] = WHvX64RegisterSfmask;
    }

//#ifdef LOG_ENABLED
//    const CPUMCPUVENDOR enmCpuVendor = CPUMGetHostCpuVendor(pGVM->pVM);
//#endif
    if (fWhat & CPUMCTX_EXTRN_OTHER_MSRS)
    {
        aenmNames[iReg++] = WHvX64RegisterApicBase; /// @todo APIC BASE
        aenmNames[iReg++] = WHvX64RegisterPat;
#if 0 /*def LOG_ENABLED*/ /** @todo Check if WHvX64RegisterMsrMtrrCap works... */
        aenmNames[iReg++] = WHvX64RegisterMsrMtrrCap;
#endif
        aenmNames[iReg++] = WHvX64RegisterMsrMtrrDefType;
        aenmNames[iReg++] = WHvX64RegisterMsrMtrrFix64k00000;
        aenmNames[iReg++] = WHvX64RegisterMsrMtrrFix16k80000;
        aenmNames[iReg++] = WHvX64RegisterMsrMtrrFix16kA0000;
        aenmNames[iReg++] = WHvX64RegisterMsrMtrrFix4kC0000;
        aenmNames[iReg++] = WHvX64RegisterMsrMtrrFix4kC8000;
        aenmNames[iReg++] = WHvX64RegisterMsrMtrrFix4kD0000;
        aenmNames[iReg++] = WHvX64RegisterMsrMtrrFix4kD8000;
        aenmNames[iReg++] = WHvX64RegisterMsrMtrrFix4kE0000;
        aenmNames[iReg++] = WHvX64RegisterMsrMtrrFix4kE8000;
        aenmNames[iReg++] = WHvX64RegisterMsrMtrrFix4kF0000;
        aenmNames[iReg++] = WHvX64RegisterMsrMtrrFix4kF8000;
        aenmNames[iReg++] = WHvX64RegisterTscAux;
        /** @todo look for HvX64RegisterIa32MiscEnable and HvX64RegisterIa32FeatureControl? */
//#ifdef LOG_ENABLED
//        if (enmCpuVendor != CPUMCPUVENDOR_AMD)
//            aenmNames[iReg++] = HvX64RegisterIa32FeatureControl;
//#endif
    }

    /* Interruptibility. */
    if (fWhat & (CPUMCTX_EXTRN_NEM_WIN_INHIBIT_INT | CPUMCTX_EXTRN_NEM_WIN_INHIBIT_NMI))
    {
        aenmNames[iReg++] = WHvRegisterInterruptState;
        aenmNames[iReg++] = WHvX64RegisterRip;
    }

    /* event injection */
    aenmNames[iReg++] = WHvRegisterPendingInterruption;
    aenmNames[iReg++] = WHvRegisterPendingEvent0;
    aenmNames[iReg++] = WHvRegisterPendingEvent1;

    size_t const cRegs = iReg;
    Assert(cRegs < RT_ELEMENTS(aenmNames));

    /*
     * Get the registers.
     */
    WHV_REGISTER_VALUE aValues[128];
    RT_ZERO(aValues);
    Assert(RT_ELEMENTS(aValues) >= cRegs);
    Assert(RT_ELEMENTS(aenmNames) >= cRegs);
#  ifdef NEM_WIN_INTERCEPT_NT_IO_CTLS
    Log12(("Calling WHvGetVirtualProcessorRegisters(%p, %u, %p, %u, %p)\n",
          pVM->nem.s.hPartition, pVCpu->idCpu, aenmNames, cRegs, aValues));
#  endif
    HRESULT hrc = WHvGetVirtualProcessorRegisters(pVM->nem.s.hPartition, pVCpu->idCpu, aenmNames, (uint32_t)cRegs, aValues);
    AssertLogRelMsgReturn(SUCCEEDED(hrc),
                          ("WHvGetVirtualProcessorRegisters(%p, %u,,%u,) -> %Rhrc (Last=%#x/%u)\n",
                           pVM->nem.s.hPartition, pVCpu->idCpu, cRegs, hrc, RTNtLastStatusValue(), RTNtLastErrorValue())
                          , VERR_NEM_GET_REGISTERS_FAILED);

    iReg = 0;
#  define GET_REG64(a_DstVar, a_enmName) do { \
            Assert(aenmNames[iReg] == (a_enmName)); \
            (a_DstVar) = aValues[iReg].Reg64; \
            iReg++; \
        } while (0)
#  define GET_REG64_LOG7(a_DstVar, a_enmName, a_szLogName) do { \
            Assert(aenmNames[iReg] == (a_enmName)); \
            if ((a_DstVar) != aValues[iReg].Reg64) \
                Log7(("NEM/%u: " a_szLogName " changed %RX64 -> %RX64\n", pVCpu->idCpu, (a_DstVar), aValues[iReg].Reg64)); \
            (a_DstVar) = aValues[iReg].Reg64; \
            iReg++; \
        } while (0)
#  define GET_REG128(a_DstVarLo, a_DstVarHi, a_enmName) do { \
            Assert(aenmNames[iReg] == a_enmName); \
            (a_DstVarLo) = aValues[iReg].Reg128.Low64; \
            (a_DstVarHi) = aValues[iReg].Reg128.High64; \
            iReg++; \
        } while (0)
#  define GET_SEG(a_SReg, a_enmName) do { \
            Assert(aenmNames[iReg] == (a_enmName)); \
            NEM_WIN_COPY_BACK_SEG(a_SReg, aValues[iReg].Segment); \
            iReg++; \
        } while (0)

    /* GPRs */
    if (fWhat & CPUMCTX_EXTRN_GPRS_MASK)
    {
        if (fWhat & CPUMCTX_EXTRN_RAX)
            GET_REG64(pCtx->rax, WHvX64RegisterRax);
        if (fWhat & CPUMCTX_EXTRN_RCX)
            GET_REG64(pCtx->rcx, WHvX64RegisterRcx);
        if (fWhat & CPUMCTX_EXTRN_RDX)
            GET_REG64(pCtx->rdx, WHvX64RegisterRdx);
        if (fWhat & CPUMCTX_EXTRN_RBX)
            GET_REG64(pCtx->rbx, WHvX64RegisterRbx);
        if (fWhat & CPUMCTX_EXTRN_RSP)
            GET_REG64(pCtx->rsp, WHvX64RegisterRsp);
        if (fWhat & CPUMCTX_EXTRN_RBP)
            GET_REG64(pCtx->rbp, WHvX64RegisterRbp);
        if (fWhat & CPUMCTX_EXTRN_RSI)
            GET_REG64(pCtx->rsi, WHvX64RegisterRsi);
        if (fWhat & CPUMCTX_EXTRN_RDI)
            GET_REG64(pCtx->rdi, WHvX64RegisterRdi);
        if (fWhat & CPUMCTX_EXTRN_R8_R15)
        {
            GET_REG64(pCtx->r8, WHvX64RegisterR8);
            GET_REG64(pCtx->r9, WHvX64RegisterR9);
            GET_REG64(pCtx->r10, WHvX64RegisterR10);
            GET_REG64(pCtx->r11, WHvX64RegisterR11);
            GET_REG64(pCtx->r12, WHvX64RegisterR12);
            GET_REG64(pCtx->r13, WHvX64RegisterR13);
            GET_REG64(pCtx->r14, WHvX64RegisterR14);
            GET_REG64(pCtx->r15, WHvX64RegisterR15);
        }
    }

    /* RIP & Flags */
    if (fWhat & CPUMCTX_EXTRN_RIP)
        GET_REG64(pCtx->rip, WHvX64RegisterRip);
    if (fWhat & CPUMCTX_EXTRN_RFLAGS)
        GET_REG64(pCtx->rflags.u, WHvX64RegisterRflags);

    /* Segments */
    if (fWhat & CPUMCTX_EXTRN_SREG_MASK)
    {
        if (fWhat & CPUMCTX_EXTRN_ES)
            GET_SEG(pCtx->es, WHvX64RegisterEs);
        if (fWhat & CPUMCTX_EXTRN_CS)
            GET_SEG(pCtx->cs, WHvX64RegisterCs);
        if (fWhat & CPUMCTX_EXTRN_SS)
            GET_SEG(pCtx->ss, WHvX64RegisterSs);
        if (fWhat & CPUMCTX_EXTRN_DS)
            GET_SEG(pCtx->ds, WHvX64RegisterDs);
        if (fWhat & CPUMCTX_EXTRN_FS)
            GET_SEG(pCtx->fs, WHvX64RegisterFs);
        if (fWhat & CPUMCTX_EXTRN_GS)
            GET_SEG(pCtx->gs, WHvX64RegisterGs);
    }

    /* Descriptor tables and the task segment. */
    if (fWhat & CPUMCTX_EXTRN_TABLE_MASK)
    {
        if (fWhat & CPUMCTX_EXTRN_LDTR)
            GET_SEG(pCtx->ldtr, WHvX64RegisterLdtr);

        if (fWhat & CPUMCTX_EXTRN_TR)
        {
            /* AMD-V likes loading TR with in AVAIL state, whereas intel insists on BUSY.  So,
               avoid to trigger sanity assertions around the code, always fix this. */
            GET_SEG(pCtx->tr, WHvX64RegisterTr);
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
        }
        if (fWhat & CPUMCTX_EXTRN_IDTR)
        {
            Assert(aenmNames[iReg] == WHvX64RegisterIdtr);
            pCtx->idtr.cbIdt = aValues[iReg].Table.Limit;
            pCtx->idtr.pIdt  = aValues[iReg].Table.Base;
            iReg++;
        }
        if (fWhat & CPUMCTX_EXTRN_GDTR)
        {
            Assert(aenmNames[iReg] == WHvX64RegisterGdtr);
            pCtx->gdtr.cbGdt = aValues[iReg].Table.Limit;
            pCtx->gdtr.pGdt  = aValues[iReg].Table.Base;
            iReg++;
        }
    }

    /* Control registers. */
    bool fMaybeChangedMode = false;
    bool fFlushTlb         = false;
    bool fFlushGlobalTlb   = false;
    if (fWhat & CPUMCTX_EXTRN_CR_MASK)
    {
        if (fWhat & CPUMCTX_EXTRN_CR0)
        {
            Assert(aenmNames[iReg] == WHvX64RegisterCr0);
            if (pCtx->cr0 != aValues[iReg].Reg64)
            {
                CPUMSetGuestCR0(pVCpu, aValues[iReg].Reg64);
                fMaybeChangedMode = true;
                fFlushTlb = fFlushGlobalTlb = true; /// @todo fix this
            }
            iReg++;
        }
        if (fWhat & CPUMCTX_EXTRN_CR2)
            GET_REG64(pCtx->cr2, WHvX64RegisterCr2);
        if (fWhat & CPUMCTX_EXTRN_CR3)
        {
            if (pCtx->cr3 != aValues[iReg].Reg64)
            {
                CPUMSetGuestCR3(pVCpu, aValues[iReg].Reg64);
                fFlushTlb = true;
            }
            iReg++;
        }
        if (fWhat & CPUMCTX_EXTRN_CR4)
        {
            if (pCtx->cr4 != aValues[iReg].Reg64)
            {
                CPUMSetGuestCR4(pVCpu, aValues[iReg].Reg64);
                fMaybeChangedMode = true;
                fFlushTlb = fFlushGlobalTlb = true; /// @todo fix this
            }
            iReg++;
        }
    }

    /// @todo CR8/TPR
    Assert(aenmNames[iReg] == WHvX64RegisterCr8);
    APICSetTpr(pVCpu, (uint8_t)aValues[iReg].Reg64 << 4);
    iReg++;

    /* Debug registers. */
    /** @todo fixme */
    if (fWhat & CPUMCTX_EXTRN_DR0_DR3)
    {
        Assert(aenmNames[iReg] == WHvX64RegisterDr0);
        Assert(aenmNames[iReg+3] == WHvX64RegisterDr3);
        if (pCtx->dr[0] != aValues[iReg].Reg64)
            CPUMSetGuestDR0(pVCpu, aValues[iReg].Reg64);
        iReg++;
        if (pCtx->dr[1] != aValues[iReg].Reg64)
            CPUMSetGuestDR1(pVCpu, aValues[iReg].Reg64);
        iReg++;
        if (pCtx->dr[2] != aValues[iReg].Reg64)
            CPUMSetGuestDR2(pVCpu, aValues[iReg].Reg64);
        iReg++;
        if (pCtx->dr[3] != aValues[iReg].Reg64)
            CPUMSetGuestDR3(pVCpu, aValues[iReg].Reg64);
        iReg++;
    }
    if (fWhat & CPUMCTX_EXTRN_DR6)
    {
        Assert(aenmNames[iReg] == WHvX64RegisterDr6);
        if (pCtx->dr[6] != aValues[iReg].Reg64)
            CPUMSetGuestDR6(pVCpu, aValues[iReg].Reg64);
        iReg++;
    }
    if (fWhat & CPUMCTX_EXTRN_DR7)
    {
        Assert(aenmNames[iReg] == WHvX64RegisterDr7);
        if (pCtx->dr[7] != aValues[iReg].Reg64)
            CPUMSetGuestDR7(pVCpu, aValues[iReg].Reg64);
        iReg++;
    }

    /* Floating point state. */
    if (fWhat & CPUMCTX_EXTRN_X87)
    {
        GET_REG128(pCtx->pXStateR3->x87.aRegs[0].au64[0], pCtx->pXStateR3->x87.aRegs[0].au64[1], WHvX64RegisterFpMmx0);
        GET_REG128(pCtx->pXStateR3->x87.aRegs[1].au64[0], pCtx->pXStateR3->x87.aRegs[1].au64[1], WHvX64RegisterFpMmx1);
        GET_REG128(pCtx->pXStateR3->x87.aRegs[2].au64[0], pCtx->pXStateR3->x87.aRegs[2].au64[1], WHvX64RegisterFpMmx2);
        GET_REG128(pCtx->pXStateR3->x87.aRegs[3].au64[0], pCtx->pXStateR3->x87.aRegs[3].au64[1], WHvX64RegisterFpMmx3);
        GET_REG128(pCtx->pXStateR3->x87.aRegs[4].au64[0], pCtx->pXStateR3->x87.aRegs[4].au64[1], WHvX64RegisterFpMmx4);
        GET_REG128(pCtx->pXStateR3->x87.aRegs[5].au64[0], pCtx->pXStateR3->x87.aRegs[5].au64[1], WHvX64RegisterFpMmx5);
        GET_REG128(pCtx->pXStateR3->x87.aRegs[6].au64[0], pCtx->pXStateR3->x87.aRegs[6].au64[1], WHvX64RegisterFpMmx6);
        GET_REG128(pCtx->pXStateR3->x87.aRegs[7].au64[0], pCtx->pXStateR3->x87.aRegs[7].au64[1], WHvX64RegisterFpMmx7);

        Assert(aenmNames[iReg] == WHvX64RegisterFpControlStatus);
        pCtx->pXStateR3->x87.FCW        = aValues[iReg].FpControlStatus.FpControl;
        pCtx->pXStateR3->x87.FSW        = aValues[iReg].FpControlStatus.FpStatus;
        pCtx->pXStateR3->x87.FTW        = aValues[iReg].FpControlStatus.FpTag
                                        /*| (aValues[iReg].FpControlStatus.Reserved << 8)*/;
        pCtx->pXStateR3->x87.FOP        = aValues[iReg].FpControlStatus.LastFpOp;
        pCtx->pXStateR3->x87.FPUIP      = (uint32_t)aValues[iReg].FpControlStatus.LastFpRip;
        pCtx->pXStateR3->x87.CS         = (uint16_t)(aValues[iReg].FpControlStatus.LastFpRip >> 32);
        pCtx->pXStateR3->x87.Rsrvd1     = (uint16_t)(aValues[iReg].FpControlStatus.LastFpRip >> 48);
        iReg++;
    }

    if (fWhat & (CPUMCTX_EXTRN_X87 | CPUMCTX_EXTRN_SSE_AVX))
    {
        Assert(aenmNames[iReg] == WHvX64RegisterXmmControlStatus);
        if (fWhat & CPUMCTX_EXTRN_X87)
        {
            pCtx->pXStateR3->x87.FPUDP  = (uint32_t)aValues[iReg].XmmControlStatus.LastFpRdp;
            pCtx->pXStateR3->x87.DS     = (uint16_t)(aValues[iReg].XmmControlStatus.LastFpRdp >> 32);
            pCtx->pXStateR3->x87.Rsrvd2 = (uint16_t)(aValues[iReg].XmmControlStatus.LastFpRdp >> 48);
        }
        pCtx->pXStateR3->x87.MXCSR      = aValues[iReg].XmmControlStatus.XmmStatusControl;
        pCtx->pXStateR3->x87.MXCSR_MASK = aValues[iReg].XmmControlStatus.XmmStatusControlMask; /** @todo ??? (Isn't this an output field?) */
        iReg++;
    }

    /* Vector state. */
    if (fWhat & CPUMCTX_EXTRN_SSE_AVX)
    {
        GET_REG128(pCtx->pXStateR3->x87.aXMM[ 0].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[ 0].uXmm.s.Hi, WHvX64RegisterXmm0);
        GET_REG128(pCtx->pXStateR3->x87.aXMM[ 1].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[ 1].uXmm.s.Hi, WHvX64RegisterXmm1);
        GET_REG128(pCtx->pXStateR3->x87.aXMM[ 2].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[ 2].uXmm.s.Hi, WHvX64RegisterXmm2);
        GET_REG128(pCtx->pXStateR3->x87.aXMM[ 3].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[ 3].uXmm.s.Hi, WHvX64RegisterXmm3);
        GET_REG128(pCtx->pXStateR3->x87.aXMM[ 4].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[ 4].uXmm.s.Hi, WHvX64RegisterXmm4);
        GET_REG128(pCtx->pXStateR3->x87.aXMM[ 5].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[ 5].uXmm.s.Hi, WHvX64RegisterXmm5);
        GET_REG128(pCtx->pXStateR3->x87.aXMM[ 6].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[ 6].uXmm.s.Hi, WHvX64RegisterXmm6);
        GET_REG128(pCtx->pXStateR3->x87.aXMM[ 7].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[ 7].uXmm.s.Hi, WHvX64RegisterXmm7);
        GET_REG128(pCtx->pXStateR3->x87.aXMM[ 8].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[ 8].uXmm.s.Hi, WHvX64RegisterXmm8);
        GET_REG128(pCtx->pXStateR3->x87.aXMM[ 9].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[ 9].uXmm.s.Hi, WHvX64RegisterXmm9);
        GET_REG128(pCtx->pXStateR3->x87.aXMM[10].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[10].uXmm.s.Hi, WHvX64RegisterXmm10);
        GET_REG128(pCtx->pXStateR3->x87.aXMM[11].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[11].uXmm.s.Hi, WHvX64RegisterXmm11);
        GET_REG128(pCtx->pXStateR3->x87.aXMM[12].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[12].uXmm.s.Hi, WHvX64RegisterXmm12);
        GET_REG128(pCtx->pXStateR3->x87.aXMM[13].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[13].uXmm.s.Hi, WHvX64RegisterXmm13);
        GET_REG128(pCtx->pXStateR3->x87.aXMM[14].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[14].uXmm.s.Hi, WHvX64RegisterXmm14);
        GET_REG128(pCtx->pXStateR3->x87.aXMM[15].uXmm.s.Lo, pCtx->pXStateR3->x87.aXMM[15].uXmm.s.Hi, WHvX64RegisterXmm15);
    }

    /* MSRs */
    // WHvX64RegisterTsc - don't touch
    if (fWhat & CPUMCTX_EXTRN_EFER)
    {
        Assert(aenmNames[iReg] == WHvX64RegisterEfer);
        if (aValues[iReg].Reg64 != pCtx->msrEFER)
        {
            Log7(("NEM/%u: MSR EFER changed %RX64 -> %RX64\n", pVCpu->idCpu, pCtx->msrEFER, aValues[iReg].Reg64));
            if ((aValues[iReg].Reg64 ^ pCtx->msrEFER) & MSR_K6_EFER_NXE)
                PGMNotifyNxeChanged(pVCpu, RT_BOOL(aValues[iReg].Reg64 & MSR_K6_EFER_NXE));
            pCtx->msrEFER = aValues[iReg].Reg64;
            fMaybeChangedMode = true;
        }
        iReg++;
    }
    if (fWhat & CPUMCTX_EXTRN_KERNEL_GS_BASE)
        GET_REG64_LOG7(pCtx->msrKERNELGSBASE, WHvX64RegisterKernelGsBase, "MSR KERNEL_GS_BASE");
    if (fWhat & CPUMCTX_EXTRN_SYSENTER_MSRS)
    {
        GET_REG64_LOG7(pCtx->SysEnter.cs,  WHvX64RegisterSysenterCs,  "MSR SYSENTER.CS");
        GET_REG64_LOG7(pCtx->SysEnter.eip, WHvX64RegisterSysenterEip, "MSR SYSENTER.EIP");
        GET_REG64_LOG7(pCtx->SysEnter.esp, WHvX64RegisterSysenterEsp, "MSR SYSENTER.ESP");
    }
    if (fWhat & CPUMCTX_EXTRN_SYSCALL_MSRS)
    {
        GET_REG64_LOG7(pCtx->msrSTAR,   WHvX64RegisterStar,   "MSR STAR");
        GET_REG64_LOG7(pCtx->msrLSTAR,  WHvX64RegisterLstar,  "MSR LSTAR");
        GET_REG64_LOG7(pCtx->msrCSTAR,  WHvX64RegisterCstar,  "MSR CSTAR");
        GET_REG64_LOG7(pCtx->msrSFMASK, WHvX64RegisterSfmask, "MSR SFMASK");
    }
    if (fWhat & CPUMCTX_EXTRN_OTHER_MSRS)
    {
        Assert(aenmNames[iReg] == WHvX64RegisterApicBase);
        const uint64_t uOldBase = APICGetBaseMsrNoCheck(pVCpu);
        if (aValues[iReg].Reg64 != uOldBase)
        {
            Log7(("NEM/%u: MSR APICBase changed %RX64 -> %RX64 (%RX64)\n",
                  pVCpu->idCpu, uOldBase, aValues[iReg].Reg64, aValues[iReg].Reg64 ^ uOldBase));
            VBOXSTRICTRC rc2 = APICSetBaseMsr(pVCpu, aValues[iReg].Reg64);
            AssertLogRelMsg(rc2 == VINF_SUCCESS, ("%Rrc %RX64\n", VBOXSTRICTRC_VAL(rc2), aValues[iReg].Reg64));
        }
        iReg++;

        GET_REG64_LOG7(pCtx->msrPAT, WHvX64RegisterPat, "MSR PAT");
#if 0 /*def LOG_ENABLED*/ /** @todo something's wrong with HvX64RegisterMtrrCap? (AMD) */
        GET_REG64_LOG7(pCtx->msrPAT, WHvX64RegisterMsrMtrrCap);
#endif
        PCPUMCTXMSRS pCtxMsrs = CPUMQueryGuestCtxMsrsPtr(pVCpu);
        GET_REG64_LOG7(pCtxMsrs->msr.MtrrDefType,      WHvX64RegisterMsrMtrrDefType,     "MSR MTRR_DEF_TYPE");
        GET_REG64_LOG7(pCtxMsrs->msr.MtrrFix64K_00000, WHvX64RegisterMsrMtrrFix64k00000, "MSR MTRR_FIX_64K_00000");
        GET_REG64_LOG7(pCtxMsrs->msr.MtrrFix16K_80000, WHvX64RegisterMsrMtrrFix16k80000, "MSR MTRR_FIX_16K_80000");
        GET_REG64_LOG7(pCtxMsrs->msr.MtrrFix16K_A0000, WHvX64RegisterMsrMtrrFix16kA0000, "MSR MTRR_FIX_16K_A0000");
        GET_REG64_LOG7(pCtxMsrs->msr.MtrrFix4K_C0000,  WHvX64RegisterMsrMtrrFix4kC0000,  "MSR MTRR_FIX_4K_C0000");
        GET_REG64_LOG7(pCtxMsrs->msr.MtrrFix4K_C8000,  WHvX64RegisterMsrMtrrFix4kC8000,  "MSR MTRR_FIX_4K_C8000");
        GET_REG64_LOG7(pCtxMsrs->msr.MtrrFix4K_D0000,  WHvX64RegisterMsrMtrrFix4kD0000,  "MSR MTRR_FIX_4K_D0000");
        GET_REG64_LOG7(pCtxMsrs->msr.MtrrFix4K_D8000,  WHvX64RegisterMsrMtrrFix4kD8000,  "MSR MTRR_FIX_4K_D8000");
        GET_REG64_LOG7(pCtxMsrs->msr.MtrrFix4K_E0000,  WHvX64RegisterMsrMtrrFix4kE0000,  "MSR MTRR_FIX_4K_E0000");
        GET_REG64_LOG7(pCtxMsrs->msr.MtrrFix4K_E8000,  WHvX64RegisterMsrMtrrFix4kE8000,  "MSR MTRR_FIX_4K_E8000");
        GET_REG64_LOG7(pCtxMsrs->msr.MtrrFix4K_F0000,  WHvX64RegisterMsrMtrrFix4kF0000,  "MSR MTRR_FIX_4K_F0000");
        GET_REG64_LOG7(pCtxMsrs->msr.MtrrFix4K_F8000,  WHvX64RegisterMsrMtrrFix4kF8000,  "MSR MTRR_FIX_4K_F8000");
        GET_REG64_LOG7(pCtxMsrs->msr.TscAux,           WHvX64RegisterTscAux,             "MSR TSC_AUX");
        /** @todo look for HvX64RegisterIa32MiscEnable and HvX64RegisterIa32FeatureControl? */
    }

    /* Interruptibility. */
    if (fWhat & (CPUMCTX_EXTRN_NEM_WIN_INHIBIT_INT | CPUMCTX_EXTRN_NEM_WIN_INHIBIT_NMI))
    {
        Assert(aenmNames[iReg] == WHvRegisterInterruptState);
        Assert(aenmNames[iReg + 1] == WHvX64RegisterRip);

        if (!(pCtx->fExtrn & CPUMCTX_EXTRN_NEM_WIN_INHIBIT_INT))
        {
            pVCpu->nem.s.fLastInterruptShadow = aValues[iReg].InterruptState.InterruptShadow;
            if (aValues[iReg].InterruptState.InterruptShadow)
                EMSetInhibitInterruptsPC(pVCpu, aValues[iReg + 1].Reg64);
            else
                VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);
        }

        if (!(pCtx->fExtrn & CPUMCTX_EXTRN_NEM_WIN_INHIBIT_NMI))
        {
            if (aValues[iReg].InterruptState.NmiMasked)
                VMCPU_FF_SET(pVCpu, VMCPU_FF_BLOCK_NMIS);
            else
                VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_BLOCK_NMIS);
        }

        fWhat |= CPUMCTX_EXTRN_NEM_WIN_INHIBIT_INT | CPUMCTX_EXTRN_NEM_WIN_INHIBIT_NMI;
        iReg += 2;
    }

    /* Event injection. */
    /// @todo WHvRegisterPendingInterruption
    Assert(aenmNames[iReg] == WHvRegisterPendingInterruption);
    if (aValues[iReg].PendingInterruption.InterruptionPending)
    {
        Log7(("PendingInterruption: type=%u vector=%#x errcd=%RTbool/%#x instr-len=%u nested=%u\n",
              aValues[iReg].PendingInterruption.InterruptionType, aValues[iReg].PendingInterruption.InterruptionVector,
              aValues[iReg].PendingInterruption.DeliverErrorCode, aValues[iReg].PendingInterruption.ErrorCode,
              aValues[iReg].PendingInterruption.InstructionLength, aValues[iReg].PendingInterruption.NestedEvent));
        AssertMsg((aValues[iReg].PendingInterruption.AsUINT64 & UINT64_C(0xfc00)) == 0,
                  ("%#RX64\n", aValues[iReg].PendingInterruption.AsUINT64));
    }

    /// @todo WHvRegisterPendingEvent0
    /// @todo WHvRegisterPendingEvent1

    /* Almost done, just update extrn flags and maybe change PGM mode. */
    pCtx->fExtrn &= ~fWhat;

    /* Typical. */
    if (!fMaybeChangedMode && !fFlushTlb)
        return VINF_SUCCESS;

    /*
     * Slow.
     */
    if (fMaybeChangedMode)
    {
        int rc = PGMChangeMode(pVCpu, pCtx->cr0, pCtx->cr4, pCtx->msrEFER);
        AssertMsg(rc == VINF_SUCCESS, ("rc=%Rrc\n", rc));
    }

    if (fFlushTlb)
    {
        int rc = PGMFlushTLB(pVCpu, pCtx->cr3, fFlushGlobalTlb);
        AssertMsg(rc == VINF_SUCCESS, ("rc=%Rrc\n", rc));
    }

    return VINF_SUCCESS;
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


#ifdef LOG_ENABLED
/** Macro used by nemHCWinExecStateToLogStr and nemR3WinExecStateToLogStr. */
# define SWITCH_IT(a_szPrefix) \
    do \
        switch (u)\
        { \
            case 0x00: return a_szPrefix ""; \
            case 0x01: return a_szPrefix ",Pnd"; \
            case 0x02: return a_szPrefix ",Dbg"; \
            case 0x03: return a_szPrefix ",Pnd,Dbg"; \
            case 0x04: return a_szPrefix ",Shw"; \
            case 0x05: return a_szPrefix ",Pnd,Shw"; \
            case 0x06: return a_szPrefix ",Shw,Dbg"; \
            case 0x07: return a_szPrefix ",Pnd,Shw,Dbg"; \
            default: AssertFailedReturn("WTF?"); \
        } \
    while (0)

# ifdef NEM_WIN_USE_OUR_OWN_RUN_API
/**
 * Translates the execution stat bitfield into a short log string, VID version.
 *
 * @returns Read-only log string.
 * @param   pMsgHdr       The header which state to summarize.
 */
static const char *nemHCWinExecStateToLogStr(HV_X64_INTERCEPT_MESSAGE_HEADER const *pMsgHdr)
{
    unsigned u = (unsigned)pMsgHdr->ExecutionState.InterruptionPending
               | ((unsigned)pMsgHdr->ExecutionState.DebugActive << 1)
               | ((unsigned)pMsgHdr->ExecutionState.InterruptShadow << 2);
    if (pMsgHdr->ExecutionState.EferLma)
        SWITCH_IT("LM");
    else if (pMsgHdr->ExecutionState.Cr0Pe)
        SWITCH_IT("PM");
    else
        SWITCH_IT("RM");
}
# elif defined(IN_RING3)
/**
 * Translates the execution stat bitfield into a short log string, WinHv version.
 *
 * @returns Read-only log string.
 * @param   pExitCtx        The exit context which state to summarize.
 */
static const char *nemR3WinExecStateToLogStr(WHV_VP_EXIT_CONTEXT const *pExitCtx)
{
    unsigned u = (unsigned)pExitCtx->ExecutionState.InterruptionPending
               | ((unsigned)pExitCtx->ExecutionState.DebugActive << 1)
               | ((unsigned)pExitCtx->ExecutionState.InterruptShadow << 2);
    if (pExitCtx->ExecutionState.EferLma)
        SWITCH_IT("LM");
    else if (pExitCtx->ExecutionState.Cr0Pe)
        SWITCH_IT("PM");
    else
        SWITCH_IT("RM");
}
# endif /* IN_RING3 && !NEM_WIN_USE_OUR_OWN_RUN_API */
# undef SWITCH_IT
#endif /* LOG_ENABLED */


#ifdef NEM_WIN_USE_OUR_OWN_RUN_API
/**
 * Advances the guest RIP and clear EFLAGS.RF, VID version.
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
#elif defined(IN_RING3)
/**
 * Advances the guest RIP and clear EFLAGS.RF, WinHv version.
 *
 * This may clear VMCPU_FF_INHIBIT_INTERRUPTS.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pCtx            The CPU context to update.
 * @param   pExitCtx        The exit context.
 */
DECLINLINE(void) nemR3WinAdvanceGuestRipAndClearRF(PVMCPU pVCpu, PCPUMCTX pCtx, WHV_VP_EXIT_CONTEXT const *pExitCtx)
{
    Assert(!(pCtx->fExtrn & (CPUMCTX_EXTRN_RIP | CPUMCTX_EXTRN_RFLAGS)));

    /* Advance the RIP. */
    Assert(pExitCtx->InstructionLength > 0 && pExitCtx->InstructionLength < 16);
    pCtx->rip += pExitCtx->InstructionLength;
    pCtx->rflags.Bits.u1RF = 0;

    /* Update interrupt inhibition. */
    if (!VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS))
    { /* likely */ }
    else if (pCtx->rip != EMGetInhibitInterruptsPC(pVCpu))
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);
}
#endif /* IN_RING3 && !NEM_WIN_USE_OUR_OWN_RUN_API */



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
            AssertLogRelMsgFailedReturn(("u2State=%#x\n", u2State), VERR_NEM_IPE_4);
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



#if defined(IN_RING0) && defined(NEM_WIN_USE_OUR_OWN_RUN_API)
/**
 * Wrapper around nemR0WinImportState that converts VERR_NEM_CHANGE_PGM_MODE and
 * VERR_NEM_FLUSH_TBL into informational status codes and logs+asserts statuses.
 *
 * @returns VBox strict status code.
 * @param   pGVM            The global (ring-0) VM structure.
 * @param   pGVCpu          The global (ring-0) per CPU structure.
 * @param   pCtx            The CPU context to import into.
 * @param   fWhat           What to import.
 * @param   pszCaller       Who is doing the importing.
 */
DECLINLINE(VBOXSTRICTRC) nemR0WinImportStateStrict(PGVM pGVM, PGVMCPU pGVCpu, PCPUMCTX pCtx, uint64_t fWhat, const char *pszCaller)
{
    int rc = nemR0WinImportState(pGVM, pGVCpu, pCtx, fWhat);
    if (RT_SUCCESS(rc))
    {
        Assert(rc == VINF_SUCCESS);
        return VINF_SUCCESS;
    }

    if (rc == VERR_NEM_CHANGE_PGM_MODE || rc == VERR_NEM_FLUSH_TLB || rc == VERR_NEM_UPDATE_APIC_BASE)
    {
        Log4(("%s/%u: nemR0WinImportState -> %Rrc\n", pszCaller, pGVCpu->idCpu, -rc));
        return -rc;
    }
    RT_NOREF(pszCaller);
    AssertMsgFailedReturn(("%s/%u: nemR0WinImportState failed: %Rrc\n", pszCaller, pGVCpu->idCpu, rc), rc);
}
#endif /* IN_RING0 && NEM_WIN_USE_OUR_OWN_RUN_API*/

#if defined(NEM_WIN_USE_OUR_OWN_RUN_API) || defined(IN_RING3)
/**
 * Wrapper around nemR0WinImportStateStrict and nemHCWinCopyStateFromHyperV.
 *
 * Unlike the wrapped APIs, this checks whether it's necessary.
 *
 * @returns VBox strict status code.
 * @param   pGVM            The global (ring-0) VM structure.
 * @param   pGVCpu          The global (ring-0) per CPU structure.
 * @param   pCtx            The CPU context to import into.
 * @param   fWhat           What to import.
 * @param   pszCaller       Who is doing the importing.
 */
DECLINLINE(VBOXSTRICTRC) nemHCWinImportStateIfNeededStrict(PVMCPU pVCpu, PGVMCPU pGVCpu, PCPUMCTX pCtx,
                                                           uint64_t fWhat, const char *pszCaller)
{
    if (pCtx->fExtrn & fWhat)
    {
#ifdef IN_RING0
        RT_NOREF(pVCpu);
        return nemR0WinImportStateStrict(pGVCpu->pGVM, pGVCpu, pCtx, fWhat, pszCaller);
#else
        RT_NOREF(pGVCpu, pszCaller);
        int rc = nemHCWinCopyStateFromHyperV(pVCpu->pVMR3, pVCpu, pCtx, NEM_WIN_CPUMCTX_EXTRN_MASK_FOR_IEM);
        AssertRCReturn(rc, rc);
#endif
    }
    return VINF_SUCCESS;
}
#endif /* NEM_WIN_USE_OUR_OWN_RUN_API || IN_RING3 */

#ifdef NEM_WIN_USE_OUR_OWN_RUN_API
/**
 * Copies register state from the X64 intercept message header.
 *
 * ASSUMES no state copied yet.
 *
 * @param   pVCpu           The cross context per CPU structure.
 * @param   pCtx            The registe rcontext.
 * @param   pHdr            The X64 intercept message header.
 * @sa      nemR3WinCopyStateFromX64Header
 */
DECLINLINE(void) nemHCWinCopyStateFromX64Header(PVMCPU pVCpu, PCPUMCTX pCtx, HV_X64_INTERCEPT_MESSAGE_HEADER const *pHdr)
{
    Assert(    (pCtx->fExtrn & (CPUMCTX_EXTRN_RIP | CPUMCTX_EXTRN_RFLAGS | CPUMCTX_EXTRN_CS | CPUMCTX_EXTRN_NEM_WIN_INHIBIT_INT))
            ==                 (CPUMCTX_EXTRN_RIP | CPUMCTX_EXTRN_RFLAGS | CPUMCTX_EXTRN_CS | CPUMCTX_EXTRN_NEM_WIN_INHIBIT_INT));
    NEM_WIN_COPY_BACK_SEG(pCtx->cs, pHdr->CsSegment);
    pCtx->rip      = pHdr->Rip;
    pCtx->rflags.u = pHdr->Rflags;

    pVCpu->nem.s.fLastInterruptShadow = pHdr->ExecutionState.InterruptShadow;
    if (!pHdr->ExecutionState.InterruptShadow)
    {
        if (!VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS))
        { /* likely */ }
        else
            VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);
    }
    else
        EMSetInhibitInterruptsPC(pVCpu, pHdr->Rip);

    pCtx->fExtrn &= ~(CPUMCTX_EXTRN_RIP | CPUMCTX_EXTRN_RFLAGS | CPUMCTX_EXTRN_CS | CPUMCTX_EXTRN_NEM_WIN_INHIBIT_INT);
}
#elif defined(IN_RING3)
/**
 * Copies register state from the (common) exit context.
 *
 * ASSUMES no state copied yet.
 *
 * @param   pVCpu           The cross context per CPU structure.
 * @param   pCtx            The registe rcontext.
 * @param   pExitCtx        The common exit context.
 * @sa      nemHCWinCopyStateFromX64Header
 */
DECLINLINE(void) nemR3WinCopyStateFromX64Header(PVMCPU pVCpu, PCPUMCTX pCtx, WHV_VP_EXIT_CONTEXT const *pExitCtx)
{
    Assert(    (pCtx->fExtrn & (CPUMCTX_EXTRN_RIP | CPUMCTX_EXTRN_RFLAGS | CPUMCTX_EXTRN_CS | CPUMCTX_EXTRN_NEM_WIN_INHIBIT_INT))
            ==                 (CPUMCTX_EXTRN_RIP | CPUMCTX_EXTRN_RFLAGS | CPUMCTX_EXTRN_CS | CPUMCTX_EXTRN_NEM_WIN_INHIBIT_INT));
    NEM_WIN_COPY_BACK_SEG(pCtx->cs, pExitCtx->Cs);
    pCtx->rip      = pExitCtx->Rip;
    pCtx->rflags.u = pExitCtx->Rflags;

    pVCpu->nem.s.fLastInterruptShadow = pExitCtx->ExecutionState.InterruptShadow;
    if (!pExitCtx->ExecutionState.InterruptShadow)
    {
        if (!VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS))
        { /* likely */ }
        else
            VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);
    }
    else
        EMSetInhibitInterruptsPC(pVCpu, pExitCtx->Rip);

    pCtx->fExtrn &= ~(CPUMCTX_EXTRN_RIP | CPUMCTX_EXTRN_RFLAGS | CPUMCTX_EXTRN_CS | CPUMCTX_EXTRN_NEM_WIN_INHIBIT_INT);
}
#endif /* IN_RING3 && !NEM_WIN_USE_OUR_OWN_RUN_API */


#ifdef NEM_WIN_USE_OUR_OWN_RUN_API
/**
 * Deals with memory intercept message.
 *
 * @returns Strict VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context per CPU structure.
 * @param   pMsg            The message.
 * @param   pCtx            The register context.
 * @param   pGVCpu          The global (ring-0) per CPU structure (NULL in r3).
 * @sa      nemR3WinHandleExitMemory
 */
NEM_TMPL_STATIC VBOXSTRICTRC
nemHCWinHandleMessageMemory(PVM pVM, PVMCPU pVCpu, HV_X64_MEMORY_INTERCEPT_MESSAGE const *pMsg, PCPUMCTX pCtx, PGVMCPU pGVCpu)
{
    Assert(   pMsg->Header.InterceptAccessType == HV_INTERCEPT_ACCESS_READ
           || pMsg->Header.InterceptAccessType == HV_INTERCEPT_ACCESS_WRITE
           || pMsg->Header.InterceptAccessType == HV_INTERCEPT_ACCESS_EXECUTE);
    AssertMsg(pMsg->Header.InstructionLength < 0x10, ("%#x\n", pMsg->Header.InstructionLength));

    /*
     * Whatever we do, we must clear pending event injection upon resume.
     */
    if (pMsg->Header.ExecutionState.InterruptionPending)
        pCtx->fExtrn &= ~CPUMCTX_EXTRN_NEM_WIN_EVENT_INJECT;

#if 0 /* Experiment: 20K -> 34K exit/s. */
    if (   pMsg->Header.ExecutionState.EferLma
        && pMsg->Header.CsSegment.Long
        && pMsg->Header.InterceptAccessType == HV_INTERCEPT_ACCESS_WRITE)
    {
        if (   pMsg->Header.Rip - (uint64_t)0xf65a < (uint64_t)(0xf662 - 0xf65a)
            && pMsg->InstructionBytes[0] == 0x89
            && pMsg->InstructionBytes[1] == 0x03)
        {
            pCtx->rip    = pMsg->Header.Rip + 2;
            pCtx->fExtrn &= ~CPUMCTX_EXTRN_RIP;
            AssertMsg(pMsg->Header.InstructionLength == 2, ("%#x\n", pMsg->Header.InstructionLength));
            //Log(("%RX64 msg:\n%.80Rhxd\n", pCtx->rip, pMsg));
            return VINF_SUCCESS;
        }
    }
#endif

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
                Log4(("MemExit/%u: %04x:%08RX64/%s: %RGp (=>%RHp) %s fProt=%u%s%s%s; restarting (%s)\n",
                      pVCpu->idCpu, pMsg->Header.CsSegment.Selector, pMsg->Header.Rip, nemHCWinExecStateToLogStr(&pMsg->Header),
                      pMsg->GuestPhysicalAddress, Info.HCPhys, g_apszPageStates[Info.u2NemState], Info.fNemProt,
                      Info.fHasHandlers ? " handlers" : "", Info.fZeroPage    ? " zero-pg" : "",
                      State.fDidSomething ? "" : " no-change", g_apszHvInterceptAccessTypes[pMsg->Header.InterceptAccessType]));
                return VINF_SUCCESS;
            }
        }
        Log4(("MemExit/%u: %04x:%08RX64/%s: %RGp (=>%RHp) %s fProt=%u%s%s%s; emulating (%s)\n",
              pVCpu->idCpu, pMsg->Header.CsSegment.Selector, pMsg->Header.Rip, nemHCWinExecStateToLogStr(&pMsg->Header),
              pMsg->GuestPhysicalAddress, Info.HCPhys, g_apszPageStates[Info.u2NemState], Info.fNemProt,
              Info.fHasHandlers ? " handlers" : "", Info.fZeroPage    ? " zero-pg" : "",
              State.fDidSomething ? "" : " no-change", g_apszHvInterceptAccessTypes[pMsg->Header.InterceptAccessType]));
    }
    else
        Log4(("MemExit/%u: %04x:%08RX64/%s: %RGp rc=%Rrc%s; emulating (%s)\n",
              pVCpu->idCpu, pMsg->Header.CsSegment.Selector, pMsg->Header.Rip, nemHCWinExecStateToLogStr(&pMsg->Header),
              pMsg->GuestPhysicalAddress, rc, State.fDidSomething ? " modified-backing" : "",
              g_apszHvInterceptAccessTypes[pMsg->Header.InterceptAccessType]));

    /*
     * Emulate the memory access, either access handler or special memory.
     */
    nemHCWinCopyStateFromX64Header(pVCpu, pCtx, &pMsg->Header);
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

    if (pMsg->Reserved1)
        Log(("MemExit/Reserved1=%#x\n", pMsg->Reserved1));
    if (pMsg->Header.ExecutionState.Reserved0 || pMsg->Header.ExecutionState.Reserved1)
        Log(("MemExit/Hdr/State: Reserved0=%#x Reserved1=%#x\n", pMsg->Header.ExecutionState.Reserved0, pMsg->Header.ExecutionState.Reserved1));
    //if (pMsg->InstructionByteCount > 0)
    //    Log4(("InstructionByteCount=%#x %.16Rhxs\n", pMsg->InstructionByteCount, pMsg->InstructionBytes));

    if (pMsg->InstructionByteCount > 0)
        rcStrict = IEMExecOneWithPrefetchedByPC(pVCpu, CPUMCTX2CORE(pCtx), pMsg->Header.Rip,
                                                pMsg->InstructionBytes, pMsg->InstructionByteCount);
    else
        rcStrict = IEMExecOne(pVCpu);
    /** @todo do we need to do anything wrt debugging here?   */
    return rcStrict;
}
#elif defined(IN_RING3)
/**
 * Deals with memory access exits (WHvRunVpExitReasonMemoryAccess).
 *
 * @returns Strict VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context per CPU structure.
 * @param   pExit           The VM exit information to handle.
 * @param   pCtx            The register context.
 * @sa      nemHCWinHandleMessageMemory
 */
NEM_TMPL_STATIC VBOXSTRICTRC
nemR3WinHandleExitMemory(PVM pVM, PVMCPU pVCpu, WHV_RUN_VP_EXIT_CONTEXT const *pExit, PCPUMCTX pCtx)
{
    Assert(pExit->MemoryAccess.AccessInfo.AccessType != 3);
    AssertMsg(pExit->VpContext.InstructionLength < 0x10, ("%#x\n", pExit->VpContext.InstructionLength));

    /*
     * Whatever we do, we must clear pending event injection upon resume.
     */
    if (pExit->VpContext.ExecutionState.InterruptionPending)
        pCtx->fExtrn &= ~CPUMCTX_EXTRN_NEM_WIN_EVENT_INJECT;

    /*
     * Ask PGM for information about the given GCPhys.  We need to check if we're
     * out of sync first.
     */
    NEMHCWINHMACPCCSTATE State = { pExit->MemoryAccess.AccessInfo.AccessType == WHvMemoryAccessWrite, false, false };
    PGMPHYSNEMPAGEINFO   Info;
    int rc = PGMPhysNemPageInfoChecker(pVM, pVCpu, pExit->MemoryAccess.Gpa, State.fWriteAccess, &Info,
                                       nemHCWinHandleMemoryAccessPageCheckerCallback, &State);
    if (RT_SUCCESS(rc))
    {
        if (Info.fNemProt & (  pExit->MemoryAccess.AccessInfo.AccessType == WHvMemoryAccessWrite
                             ? NEM_PAGE_PROT_WRITE : NEM_PAGE_PROT_READ))
        {
            if (State.fCanResume)
            {
                Log4(("MemExit/%u: %04x:%08RX64/%s: %RGp (=>%RHp) %s fProt=%u%s%s%s; restarting (%s)\n",
                      pVCpu->idCpu, pExit->VpContext.Cs.Selector, pExit->VpContext.Rip, nemR3WinExecStateToLogStr(&pExit->VpContext),
                      pExit->MemoryAccess.Gpa, Info.HCPhys, g_apszPageStates[Info.u2NemState], Info.fNemProt,
                      Info.fHasHandlers ? " handlers" : "", Info.fZeroPage    ? " zero-pg" : "",
                      State.fDidSomething ? "" : " no-change", g_apszHvInterceptAccessTypes[pExit->MemoryAccess.AccessInfo.AccessType]));
                return VINF_SUCCESS;
            }
        }
        Log4(("MemExit/%u: %04x:%08RX64/%s: %RGp (=>%RHp) %s fProt=%u%s%s%s; emulating (%s)\n",
              pVCpu->idCpu, pExit->VpContext.Cs.Selector, pExit->VpContext.Rip, nemR3WinExecStateToLogStr(&pExit->VpContext),
              pExit->MemoryAccess.Gpa, Info.HCPhys, g_apszPageStates[Info.u2NemState], Info.fNemProt,
              Info.fHasHandlers ? " handlers" : "", Info.fZeroPage    ? " zero-pg" : "",
              State.fDidSomething ? "" : " no-change", g_apszHvInterceptAccessTypes[pExit->MemoryAccess.AccessInfo.AccessType]));
    }
    else
        Log4(("MemExit/%u: %04x:%08RX64/%s: %RGp rc=%Rrc%s; emulating (%s)\n",
              pVCpu->idCpu, pExit->VpContext.Cs.Selector, pExit->VpContext.Rip, nemR3WinExecStateToLogStr(&pExit->VpContext),
              pExit->MemoryAccess.Gpa, rc, State.fDidSomething ? " modified-backing" : "",
              g_apszHvInterceptAccessTypes[pExit->MemoryAccess.AccessInfo.AccessType]));

    /*
     * Emulate the memory access, either access handler or special memory.
     */
    nemR3WinCopyStateFromX64Header(pVCpu, pCtx, &pExit->VpContext);
    rc = nemHCWinCopyStateFromHyperV(pVM, pVCpu, pCtx, NEM_WIN_CPUMCTX_EXTRN_MASK_FOR_IEM);
    AssertRCReturn(rc, rc);

    if (pExit->VpContext.ExecutionState.Reserved0 || pExit->VpContext.ExecutionState.Reserved1)
        Log(("MemExit/Hdr/State: Reserved0=%#x Reserved1=%#x\n", pExit->VpContext.ExecutionState.Reserved0, pExit->VpContext.ExecutionState.Reserved1));
    //if (pMsg->InstructionByteCount > 0)
    //    Log4(("InstructionByteCount=%#x %.16Rhxs\n", pMsg->InstructionByteCount, pMsg->InstructionBytes));

    VBOXSTRICTRC rcStrict;
    if (pExit->MemoryAccess.InstructionByteCount > 0)
        rcStrict = IEMExecOneWithPrefetchedByPC(pVCpu, CPUMCTX2CORE(pCtx), pExit->VpContext.Rip,
                                                pExit->MemoryAccess.InstructionBytes, pExit->MemoryAccess.InstructionByteCount);
    else
        rcStrict = IEMExecOne(pVCpu);
    /** @todo do we need to do anything wrt debugging here?   */
    return rcStrict;
}
#endif /* IN_RING3 && !NEM_WIN_USE_OUR_OWN_RUN_API */


#ifdef NEM_WIN_USE_OUR_OWN_RUN_API
/**
 * Deals with I/O port intercept message.
 *
 * @returns Strict VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context per CPU structure.
 * @param   pMsg            The message.
 * @param   pCtx            The register context.
 * @param   pGVCpu          The global (ring-0) per CPU structure (NULL in r3).
 */
NEM_TMPL_STATIC VBOXSTRICTRC
nemHCWinHandleMessageIoPort(PVM pVM, PVMCPU pVCpu, HV_X64_IO_PORT_INTERCEPT_MESSAGE const *pMsg, PCPUMCTX pCtx, PGVMCPU pGVCpu)
{
    Assert(   pMsg->AccessInfo.AccessSize == 1
           || pMsg->AccessInfo.AccessSize == 2
           || pMsg->AccessInfo.AccessSize == 4);
    Assert(   pMsg->Header.InterceptAccessType == HV_INTERCEPT_ACCESS_READ
           || pMsg->Header.InterceptAccessType == HV_INTERCEPT_ACCESS_WRITE);
    AssertMsg(pMsg->Header.InstructionLength < 0x10, ("%#x\n", pMsg->Header.InstructionLength));

    /*
     * Whatever we do, we must clear pending event injection upon resume.
     */
    if (pMsg->Header.ExecutionState.InterruptionPending)
        pCtx->fExtrn &= ~CPUMCTX_EXTRN_NEM_WIN_EVENT_INJECT;

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
            Log4(("IOExit/%u: %04x:%08RX64/%s: OUT %#x, %#x LB %u rcStrict=%Rrc\n",
                  pVCpu->idCpu, pMsg->Header.CsSegment.Selector, pMsg->Header.Rip, nemHCWinExecStateToLogStr(&pMsg->Header),
                  pMsg->PortNumber, (uint32_t)pMsg->Rax & fAndMask, pMsg->AccessInfo.AccessSize, VBOXSTRICTRC_VAL(rcStrict) ));
            if (IOM_SUCCESS(rcStrict))
            {
                nemHCWinCopyStateFromX64Header(pVCpu, pCtx, &pMsg->Header);
                nemHCWinAdvanceGuestRipAndClearRF(pVCpu, pCtx, &pMsg->Header);
            }
        }
        else
        {
            uint32_t uValue = 0;
            rcStrict = IOMIOPortRead(pVM, pVCpu, pMsg->PortNumber, &uValue, pMsg->AccessInfo.AccessSize);
            Log4(("IOExit/%u: %04x:%08RX64/%s: IN %#x LB %u -> %#x, rcStrict=%Rrc\n",
                  pVCpu->idCpu, pMsg->Header.CsSegment.Selector, pMsg->Header.Rip, nemHCWinExecStateToLogStr(&pMsg->Header),
                  pMsg->PortNumber, pMsg->AccessInfo.AccessSize, uValue, VBOXSTRICTRC_VAL(rcStrict) ));
            if (IOM_SUCCESS(rcStrict))
            {
                if (pMsg->AccessInfo.AccessSize != 4)
                    pCtx->rax = (pMsg->Rax & ~(uint64_t)fAndMask) | (uValue & fAndMask);
                else
                    pCtx->rax = uValue;
                pCtx->fExtrn &= ~CPUMCTX_EXTRN_RAX;
                Log4(("IOExit/%u: RAX %#RX64 -> %#RX64\n", pVCpu->idCpu, pMsg->Rax, pCtx->rax));
                nemHCWinCopyStateFromX64Header(pVCpu, pCtx, &pMsg->Header);
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
        nemHCWinCopyStateFromX64Header(pVCpu, pCtx, &pMsg->Header);
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

        Log4(("IOExit/%u: %04x:%08RX64/%s: %s%s %#x LB %u (emulating)\n",
              pVCpu->idCpu, pMsg->Header.CsSegment.Selector, pMsg->Header.Rip, nemHCWinExecStateToLogStr(&pMsg->Header),
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
        if (   pMsg->Header.ExecutionState.DebugActive /** @todo Microsoft: Does DebugActive this only reflect DR7? */
            || (pMsg->Header.Rflags & X86_EFL_TF)
            || DBGFBpIsHwIoArmed(pVM) )
        {
            /** @todo Debugging. */
        }
    }
    return rcStrict;
}
#elif defined(IN_RING3)
/**
 * Deals with I/O port access exits (WHvRunVpExitReasonX64IoPortAccess).
 *
 * @returns Strict VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context per CPU structure.
 * @param   pExit           The VM exit information to handle.
 * @param   pCtx            The register context.
 * @sa      nemHCWinHandleMessageIoPort
 */
NEM_TMPL_STATIC VBOXSTRICTRC
nemR3WinHandleExitIoPort(PVM pVM, PVMCPU pVCpu, WHV_RUN_VP_EXIT_CONTEXT const *pExit, PCPUMCTX pCtx)
{
    Assert(   pExit->IoPortAccess.AccessInfo.AccessSize == 1
           || pExit->IoPortAccess.AccessInfo.AccessSize == 2
           || pExit->IoPortAccess.AccessInfo.AccessSize == 4);
    AssertMsg(pExit->VpContext.InstructionLength < 0x10, ("%#x\n", pExit->VpContext.InstructionLength));

    /*
     * Whatever we do, we must clear pending event injection upon resume.
     */
    if (pExit->VpContext.ExecutionState.InterruptionPending)
        pCtx->fExtrn &= ~CPUMCTX_EXTRN_NEM_WIN_EVENT_INJECT;

    VBOXSTRICTRC rcStrict;
    if (!pExit->IoPortAccess.AccessInfo.StringOp)
    {
        /*
         * Simple port I/O.
         */
        static uint32_t const s_fAndMask[8] =
        {   UINT32_MAX, UINT32_C(0xff), UINT32_C(0xffff), UINT32_MAX,   UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX   };
        uint32_t const        fAndMask      = s_fAndMask[pExit->IoPortAccess.AccessInfo.AccessSize];
        if (pExit->IoPortAccess.AccessInfo.IsWrite)
        {
            rcStrict = IOMIOPortWrite(pVM, pVCpu, pExit->IoPortAccess.PortNumber, (uint32_t)pExit->IoPortAccess.Rax & fAndMask,
                                      pExit->IoPortAccess.AccessInfo.AccessSize);
            Log4(("IOExit/%u: %04x:%08RX64/%s: OUT %#x, %#x LB %u rcStrict=%Rrc\n",
                  pVCpu->idCpu, pExit->VpContext.Cs.Selector, pExit->VpContext.Rip, nemR3WinExecStateToLogStr(&pExit->VpContext),
                  pExit->IoPortAccess.PortNumber, (uint32_t)pExit->IoPortAccess.Rax & fAndMask,
                  pExit->IoPortAccess.AccessInfo.AccessSize, VBOXSTRICTRC_VAL(rcStrict) ));
            if (IOM_SUCCESS(rcStrict))
            {
                nemR3WinCopyStateFromX64Header(pVCpu, pCtx, &pExit->VpContext);
                nemR3WinAdvanceGuestRipAndClearRF(pVCpu, pCtx, &pExit->VpContext);
            }
        }
        else
        {
            uint32_t uValue = 0;
            rcStrict = IOMIOPortRead(pVM, pVCpu, pExit->IoPortAccess.PortNumber, &uValue, pExit->IoPortAccess.AccessInfo.AccessSize);
            Log4(("IOExit/%u: %04x:%08RX64/%s: IN %#x LB %u -> %#x, rcStrict=%Rrc\n",
                  pVCpu->idCpu, pExit->VpContext.Cs.Selector, pExit->VpContext.Rip, nemR3WinExecStateToLogStr(&pExit->VpContext),
                  pExit->IoPortAccess.PortNumber, pExit->IoPortAccess.AccessInfo.AccessSize, uValue, VBOXSTRICTRC_VAL(rcStrict) ));
            if (IOM_SUCCESS(rcStrict))
            {
                if (pExit->IoPortAccess.AccessInfo.AccessSize != 4)
                    pCtx->rax = (pExit->IoPortAccess.Rax & ~(uint64_t)fAndMask) | (uValue & fAndMask);
                else
                    pCtx->rax = uValue;
                pCtx->fExtrn &= ~CPUMCTX_EXTRN_RAX;
                Log4(("IOExit/%u: RAX %#RX64 -> %#RX64\n", pVCpu->idCpu, pExit->IoPortAccess.Rax, pCtx->rax));
                nemR3WinCopyStateFromX64Header(pVCpu, pCtx, &pExit->VpContext);
                nemR3WinAdvanceGuestRipAndClearRF(pVCpu, pCtx, &pExit->VpContext);
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
        nemR3WinCopyStateFromX64Header(pVCpu, pCtx, &pExit->VpContext);
        pCtx->fExtrn &= ~(  CPUMCTX_EXTRN_RAX | CPUMCTX_EXTRN_RCX | CPUMCTX_EXTRN_RDI | CPUMCTX_EXTRN_RSI
                          | CPUMCTX_EXTRN_DS  | CPUMCTX_EXTRN_ES);
        NEM_WIN_COPY_BACK_SEG(pCtx->ds, pExit->IoPortAccess.Ds);
        NEM_WIN_COPY_BACK_SEG(pCtx->es, pExit->IoPortAccess.Es);
        pCtx->rax = pExit->IoPortAccess.Rax;
        pCtx->rcx = pExit->IoPortAccess.Rcx;
        pCtx->rdi = pExit->IoPortAccess.Rdi;
        pCtx->rsi = pExit->IoPortAccess.Rsi;
# ifdef IN_RING0
        rcStrict = nemR0WinImportStateStrict(pGVCpu->pGVM, pGVCpu, pCtx, NEM_WIN_CPUMCTX_EXTRN_MASK_FOR_IEM, "IOExit");
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
# else
        int rc = nemHCWinCopyStateFromHyperV(pVM, pVCpu, pCtx, NEM_WIN_CPUMCTX_EXTRN_MASK_FOR_IEM);
        AssertRCReturn(rc, rc);
# endif

        Log4(("IOExit/%u: %04x:%08RX64/%s: %s%s %#x LB %u (emulating)\n",
              pVCpu->idCpu, pExit->VpContext.Cs.Selector, pExit->VpContext.Rip, nemR3WinExecStateToLogStr(&pExit->VpContext),
              pExit->IoPortAccess.AccessInfo.RepPrefix ? "REP " : "",
              pExit->IoPortAccess.AccessInfo.IsWrite ? "OUTS" : "INS",
              pExit->IoPortAccess.PortNumber, pExit->IoPortAccess.AccessInfo.AccessSize ));
        rcStrict = IEMExecOne(pVCpu);
    }
    if (IOM_SUCCESS(rcStrict))
    {
        /*
         * Do debug checks.
         */
        if (   pExit->VpContext.ExecutionState.DebugActive /** @todo Microsoft: Does DebugActive this only reflect DR7? */
            || (pExit->VpContext.Rflags & X86_EFL_TF)
            || DBGFBpIsHwIoArmed(pVM) )
        {
            /** @todo Debugging. */
        }
    }
    return rcStrict;

}
#endif /* IN_RING3 && !NEM_WIN_USE_OUR_OWN_RUN_API */

#ifdef NEM_WIN_USE_OUR_OWN_RUN_API
/**
 * Deals with interrupt window message.
 *
 * @returns Strict VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context per CPU structure.
 * @param   pMsg            The message.
 * @param   pCtx            The register context.
 * @param   pGVCpu          The global (ring-0) per CPU structure (NULL in r3).
 * @sa      nemR3WinHandleExitInterruptWindow
 */
NEM_TMPL_STATIC VBOXSTRICTRC
nemHCWinHandleMessageInterruptWindow(PVM pVM, PVMCPU pVCpu, HV_X64_INTERRUPT_WINDOW_MESSAGE const *pMsg,
                                     PCPUMCTX pCtx, PGVMCPU pGVCpu)
{
    /*
     * Assert message sanity.
     */
    Assert(   pMsg->Header.InterceptAccessType == HV_INTERCEPT_ACCESS_EXECUTE
           || pMsg->Header.InterceptAccessType == HV_INTERCEPT_ACCESS_READ   // READ & WRITE are probably not used here
           || pMsg->Header.InterceptAccessType == HV_INTERCEPT_ACCESS_WRITE);
    AssertMsg(pMsg->Header.InstructionLength < 0x10, ("%#x\n", pMsg->Header.InstructionLength));
    AssertMsg(pMsg->Type == HvX64PendingInterrupt || pMsg->Type == HvX64PendingNmi, ("%#x\n", pMsg->Type));

    /*
     * Just copy the state we've got and handle it in the loop for now.
     */
    nemHCWinCopyStateFromX64Header(pVCpu, pCtx, &pMsg->Header);
    Log4(("IntWinExit/%u: %04x:%08RX64/%s: %u IF=%d InterruptShadow=%d\n",
          pVCpu->idCpu, pMsg->Header.CsSegment.Selector, pMsg->Header.Rip,  nemHCWinExecStateToLogStr(&pMsg->Header),
          pMsg->Type, RT_BOOL(pMsg->Header.Rflags & X86_EFL_IF), pMsg->Header.ExecutionState.InterruptShadow));

    /** @todo call nemHCWinHandleInterruptFF   */
    RT_NOREF(pVM, pGVCpu);
    return VINF_SUCCESS;
}
#elif defined(IN_RING3)
/**
 * Deals with interrupt window exits (WHvRunVpExitReasonX64InterruptWindow).
 *
 * @returns Strict VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context per CPU structure.
 * @param   pExit           The VM exit information to handle.
 * @param   pCtx            The register context.
 * @sa      nemHCWinHandleMessageInterruptWindow
 */
NEM_TMPL_STATIC VBOXSTRICTRC
nemR3WinHandleExitInterruptWindow(PVM pVM, PVMCPU pVCpu, WHV_RUN_VP_EXIT_CONTEXT const *pExit, PCPUMCTX pCtx)
{
    /*
     * Assert message sanity.
     */
    AssertMsg(pExit->VpContext.InstructionLength < 0x10, ("%#x\n", pExit->VpContext.InstructionLength));
    AssertMsg(   pExit->InterruptWindow.DeliverableType == WHvX64PendingInterrupt
              || pExit->InterruptWindow.DeliverableType == WHvX64PendingNmi,
              ("%#x\n", pExit->InterruptWindow.DeliverableType));

    /*
     * Just copy the state we've got and handle it in the loop for now.
     */
    nemR3WinCopyStateFromX64Header(pVCpu, pCtx, &pExit->VpContext);
    Log4(("IntWinExit/%u: %04x:%08RX64/%s: %u IF=%d InterruptShadow=%d\n",
          pVCpu->idCpu, pExit->VpContext.Cs.Selector, pExit->VpContext.Rip,  nemR3WinExecStateToLogStr(&pExit->VpContext),
          pExit->InterruptWindow.DeliverableType, RT_BOOL(pExit->VpContext.Rflags & X86_EFL_IF),
          pExit->VpContext.ExecutionState.InterruptShadow));

    /** @todo call nemHCWinHandleInterruptFF   */
    RT_NOREF(pVM);
    return VINF_SUCCESS;
}
#endif /* IN_RING3 && !NEM_WIN_USE_OUR_OWN_RUN_API */

#ifdef NEM_WIN_USE_OUR_OWN_RUN_API
/**
 * Deals with CPUID intercept message.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context per CPU structure.
 * @param   pMsg            The message.
 * @param   pCtx            The register context.
 */
NEM_TMPL_STATIC VBOXSTRICTRC nemHCWinHandleMessageCpuId(PVMCPU pVCpu, HV_X64_CPUID_INTERCEPT_MESSAGE const *pMsg, PCPUMCTX pCtx)
{
    AssertMsg(pMsg->Header.InstructionLength < 0x10, ("%#x\n", pMsg->Header.InstructionLength));

    /*
     * Soak up state and execute the instruction.
     *
     * Note! If this grows slightly more complicated, combine into an IEMExecDecodedCpuId
     *       function and make everyone use it.
     */
    /** @todo Combine implementations into IEMExecDecodedCpuId as this will
     *        only get weirder with nested VT-x and AMD-V support. */
    nemHCWinCopyStateFromX64Header(pVCpu, pCtx, &pMsg->Header);

    /* Copy in the low register values (top is always cleared). */
    pCtx->rax = (uint32_t)pMsg->Rax;
    pCtx->rcx = (uint32_t)pMsg->Rcx;
    pCtx->rdx = (uint32_t)pMsg->Rdx;
    pCtx->rbx = (uint32_t)pMsg->Rbx;
    pCtx->fExtrn &= ~(CPUMCTX_EXTRN_RAX | CPUMCTX_EXTRN_RCX | CPUMCTX_EXTRN_RDX | CPUMCTX_EXTRN_RBX);

    /* Get the correct values. */
    CPUMGetGuestCpuId(pVCpu, pCtx->eax, pCtx->ecx, &pCtx->eax, &pCtx->ebx, &pCtx->ecx, &pCtx->edx);

    Log4(("CpuIdExit/%u: %04x:%08RX64/%s: rax=%08RX64 / rcx=%08RX64 / rdx=%08RX64 / rbx=%08RX64 -> %08RX32 / %08RX32 / %08RX32 / %08RX32 (hv: %08RX64 / %08RX64 / %08RX64 / %08RX64)\n",
          pVCpu->idCpu, pMsg->Header.CsSegment.Selector, pMsg->Header.Rip, nemHCWinExecStateToLogStr(&pMsg->Header),
          pMsg->Rax,                           pMsg->Rcx,              pMsg->Rdx,              pMsg->Rbx,
          pCtx->eax,                           pCtx->ecx,              pCtx->edx,              pCtx->ebx,
          pMsg->DefaultResultRax, pMsg->DefaultResultRcx, pMsg->DefaultResultRdx, pMsg->DefaultResultRbx));

    /* Move RIP and we're done. */
    nemHCWinAdvanceGuestRipAndClearRF(pVCpu, pCtx, &pMsg->Header);

    return VINF_SUCCESS;
}
#elif defined(IN_RING3)
/**
 * Deals with CPUID exits (WHvRunVpExitReasonX64Cpuid).
 *
 * @returns Strict VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context per CPU structure.
 * @param   pExit           The VM exit information to handle.
 * @param   pCtx            The register context.
 * @sa      nemHCWinHandleMessageInterruptWindow
 */
NEM_TMPL_STATIC VBOXSTRICTRC
nemR3WinHandleExitCpuId(PVM pVM, PVMCPU pVCpu, WHV_RUN_VP_EXIT_CONTEXT const *pExit, PCPUMCTX pCtx)
{
    AssertMsg(pExit->VpContext.InstructionLength < 0x10, ("%#x\n", pExit->VpContext.InstructionLength));

    /*
     * Soak up state and execute the instruction.
     *
     * Note! If this grows slightly more complicated, combine into an IEMExecDecodedCpuId
     *       function and make everyone use it.
     */
    /** @todo Combine implementations into IEMExecDecodedCpuId as this will
     *        only get weirder with nested VT-x and AMD-V support. */
    nemR3WinCopyStateFromX64Header(pVCpu, pCtx, &pExit->VpContext);

    /* Copy in the low register values (top is always cleared). */
    pCtx->rax = (uint32_t)pExit->CpuidAccess.Rax;
    pCtx->rcx = (uint32_t)pExit->CpuidAccess.Rcx;
    pCtx->rdx = (uint32_t)pExit->CpuidAccess.Rdx;
    pCtx->rbx = (uint32_t)pExit->CpuidAccess.Rbx;
    pCtx->fExtrn &= ~(CPUMCTX_EXTRN_RAX | CPUMCTX_EXTRN_RCX | CPUMCTX_EXTRN_RDX | CPUMCTX_EXTRN_RBX);

    /* Get the correct values. */
    CPUMGetGuestCpuId(pVCpu, pCtx->eax, pCtx->ecx, &pCtx->eax, &pCtx->ebx, &pCtx->ecx, &pCtx->edx);

    Log4(("CpuIdExit/%u: %04x:%08RX64/%s: rax=%08RX64 / rcx=%08RX64 / rdx=%08RX64 / rbx=%08RX64 -> %08RX32 / %08RX32 / %08RX32 / %08RX32 (hv: %08RX64 / %08RX64 / %08RX64 / %08RX64)\n",
          pVCpu->idCpu, pExit->VpContext.Cs.Selector, pExit->VpContext.Rip, nemR3WinExecStateToLogStr(&pExit->VpContext),
          pExit->CpuidAccess.Rax,                           pExit->CpuidAccess.Rcx,              pExit->CpuidAccess.Rdx,              pExit->CpuidAccess.Rbx,
          pCtx->eax,                                                     pCtx->ecx,                           pCtx->edx,                           pCtx->ebx,
          pExit->CpuidAccess.DefaultResultRax, pExit->CpuidAccess.DefaultResultRcx, pExit->CpuidAccess.DefaultResultRdx, pExit->CpuidAccess.DefaultResultRbx));

    /* Move RIP and we're done. */
    nemR3WinAdvanceGuestRipAndClearRF(pVCpu, pCtx, &pExit->VpContext);

    RT_NOREF_PV(pVM);
    return VINF_SUCCESS;
}
#endif /* IN_RING3 && !NEM_WIN_USE_OUR_OWN_RUN_API */

#ifdef NEM_WIN_USE_OUR_OWN_RUN_API
/**
 * Deals with MSR intercept message.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context per CPU structure.
 * @param   pMsg            The message.
 * @param   pCtx            The register context.
 * @param   pGVCpu          The global (ring-0) per CPU structure (NULL in r3).
 * @sa      nemR3WinHandleExitMsr
 */
NEM_TMPL_STATIC VBOXSTRICTRC nemHCWinHandleMessageMsr(PVMCPU pVCpu, HV_X64_MSR_INTERCEPT_MESSAGE const *pMsg,
                                                      PCPUMCTX pCtx, PGVMCPU pGVCpu)
{
    /*
     * A wee bit of sanity first.
     */
    AssertMsg(pMsg->Header.InstructionLength < 0x10, ("%#x\n", pMsg->Header.InstructionLength));
    Assert(   pMsg->Header.InterceptAccessType == HV_INTERCEPT_ACCESS_READ
           || pMsg->Header.InterceptAccessType == HV_INTERCEPT_ACCESS_WRITE);

    /*
     * Check CPL as that's common to both RDMSR and WRMSR.
     */
    VBOXSTRICTRC rcStrict;
    if (pMsg->Header.ExecutionState.Cpl == 0)
    {
        /*
         * Get all the MSR state.  Since we're getting EFER, we also need to
         * get CR0, CR4 and CR3.
         */
        nemHCWinCopyStateFromX64Header(pVCpu, pCtx, &pMsg->Header);
        rcStrict = nemHCWinImportStateIfNeededStrict(pVCpu, pGVCpu, pCtx,
                                                       CPUMCTX_EXTRN_ALL_MSRS | CPUMCTX_EXTRN_CR0
                                                     | CPUMCTX_EXTRN_CR3 | CPUMCTX_EXTRN_CR4,
                                                     "MSRs");
        if (rcStrict == VINF_SUCCESS)
        {

            /*
             * Handle writes.
             */
            if (pMsg->Header.InterceptAccessType == HV_INTERCEPT_ACCESS_WRITE)
            {
                rcStrict = CPUMSetGuestMsr(pVCpu, pMsg->MsrNumber, RT_MAKE_U64((uint32_t)pMsg->Rax, (uint32_t)pMsg->Rdx));
                Log4(("MsrExit/%u: %04x:%08RX64/%s: WRMSR %08x, %08x:%08x -> %Rrc\n",
                      pVCpu->idCpu, pMsg->Header.CsSegment.Selector, pMsg->Header.Rip, nemHCWinExecStateToLogStr(&pMsg->Header),
                      pMsg->MsrNumber, (uint32_t)pMsg->Rax, (uint32_t)pMsg->Rdx, VBOXSTRICTRC_VAL(rcStrict) ));
                if (rcStrict == VINF_SUCCESS)
                {
                    nemHCWinAdvanceGuestRipAndClearRF(pVCpu, pCtx, &pMsg->Header);
                    return VINF_SUCCESS;
                }
# ifndef IN_RING3
                /* move to ring-3 and handle the trap/whatever there, as we want to LogRel this. */
                if (rcStrict == VERR_CPUM_RAISE_GP_0)
                    rcStrict = VINF_CPUM_R3_MSR_WRITE;
                return rcStrict;
# else
                LogRel(("MsrExit/%u: %04x:%08RX64/%s: WRMSR %08x, %08x:%08x -> %Rrc!\n",
                        pVCpu->idCpu, pMsg->Header.CsSegment.Selector, pMsg->Header.Rip, nemHCWinExecStateToLogStr(&pMsg->Header),
                        pMsg->MsrNumber, (uint32_t)pMsg->Rax, (uint32_t)pMsg->Rdx, VBOXSTRICTRC_VAL(rcStrict) ));
# endif
            }
            /*
             * Handle reads.
             */
            else
            {
                uint64_t uValue = 0;
                rcStrict = CPUMQueryGuestMsr(pVCpu, pMsg->MsrNumber, &uValue);
                Log4(("MsrExit/%u: %04x:%08RX64/%s: RDMSR %08x -> %08RX64 / %Rrc\n",
                      pVCpu->idCpu, pMsg->Header.CsSegment.Selector, pMsg->Header.Rip, nemHCWinExecStateToLogStr(&pMsg->Header),
                      pMsg->MsrNumber, uValue, VBOXSTRICTRC_VAL(rcStrict) ));
                if (rcStrict == VINF_SUCCESS)
                {
                    pCtx->rax = (uint32_t)uValue;
                    pCtx->rdx = uValue >> 32;
                    pCtx->fExtrn &= ~(CPUMCTX_EXTRN_RAX | CPUMCTX_EXTRN_RDX);
                    nemHCWinAdvanceGuestRipAndClearRF(pVCpu, pCtx, &pMsg->Header);
                    return VINF_SUCCESS;
                }
# ifndef IN_RING3
                /* move to ring-3 and handle the trap/whatever there, as we want to LogRel this. */
                if (rcStrict == VERR_CPUM_RAISE_GP_0)
                    rcStrict = VINF_CPUM_R3_MSR_READ;
                return rcStrict;
# else
                LogRel(("MsrExit/%u: %04x:%08RX64/%s: RDMSR %08x -> %08RX64 / %Rrc\n",
                        pVCpu->idCpu, pMsg->Header.CsSegment.Selector, pMsg->Header.Rip, nemHCWinExecStateToLogStr(&pMsg->Header),
                        pMsg->MsrNumber, uValue, VBOXSTRICTRC_VAL(rcStrict) ));
# endif
            }
        }
        else
        {
            LogRel(("MsrExit/%u: %04x:%08RX64/%s: %sMSR %08x -> %Rrc - msr state import\n",
                    pVCpu->idCpu, pMsg->Header.CsSegment.Selector, pMsg->Header.Rip, nemHCWinExecStateToLogStr(&pMsg->Header),
                    pMsg->Header.InterceptAccessType == HV_INTERCEPT_ACCESS_WRITE ? "WR" : "RD",
                    pMsg->MsrNumber, VBOXSTRICTRC_VAL(rcStrict) ));
            return rcStrict;
        }
    }
    else if (pMsg->Header.InterceptAccessType == HV_INTERCEPT_ACCESS_WRITE)
        Log4(("MsrExit/%u: %04x:%08RX64/%s: CPL %u -> #GP(0); WRMSR %08x, %08x:%08x\n",
              pVCpu->idCpu, pMsg->Header.CsSegment.Selector, pMsg->Header.Rip, nemHCWinExecStateToLogStr(&pMsg->Header),
              pMsg->Header.ExecutionState.Cpl, pMsg->MsrNumber, (uint32_t)pMsg->Rax, (uint32_t)pMsg->Rdx ));
    else
        Log4(("MsrExit/%u: %04x:%08RX64/%s: CPL %u -> #GP(0); RDMSR %08x\n",
              pVCpu->idCpu, pMsg->Header.CsSegment.Selector, pMsg->Header.Rip, nemHCWinExecStateToLogStr(&pMsg->Header),
              pMsg->Header.ExecutionState.Cpl, pMsg->MsrNumber));

    /*
     * If we get down here, we're supposed to #GP(0).
     */
    rcStrict = nemHCWinImportStateIfNeededStrict(pVCpu, pGVCpu, pCtx, NEM_WIN_CPUMCTX_EXTRN_MASK_FOR_IEM, "MSR");
    if (rcStrict == VINF_SUCCESS)
    {
        rcStrict = IEMInjectTrap(pVCpu, X86_XCPT_GP, TRPM_TRAP, 0, 0, 0);
        if (rcStrict == VINF_IEM_RAISED_XCPT)
            rcStrict = VINF_SUCCESS;
        else if (rcStrict != VINF_SUCCESS)
            Log4(("MsrExit/%u: Injecting #GP(0) failed: %Rrc\n", VBOXSTRICTRC_VAL(rcStrict) ));
    }
    return rcStrict;
}
#elif defined(IN_RING3)
/**
 * Deals with MSR access exits (WHvRunVpExitReasonX64MsrAccess).
 *
 * @returns Strict VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context per CPU structure.
 * @param   pExit           The VM exit information to handle.
 * @param   pCtx            The register context.
 * @sa      nemHCWinHandleMessageMsr
 */
NEM_TMPL_STATIC VBOXSTRICTRC
nemR3WinHandleExitMsr(PVM pVM, PVMCPU pVCpu, WHV_RUN_VP_EXIT_CONTEXT const *pExit, PCPUMCTX pCtx)
{
    AssertMsg(pExit->VpContext.InstructionLength < 0x10, ("%#x\n", pExit->VpContext.InstructionLength));

    /*
     * Check CPL as that's common to both RDMSR and WRMSR.
     */
    VBOXSTRICTRC rcStrict;
    if (pExit->VpContext.ExecutionState.Cpl == 0)
    {
        /*
         * Get all the MSR state.  Since we're getting EFER, we also need to
         * get CR0, CR4 and CR3.
         */
        nemR3WinCopyStateFromX64Header(pVCpu, pCtx, &pExit->VpContext);
        rcStrict = nemHCWinImportStateIfNeededStrict(pVCpu, NULL, pCtx,
                                                       CPUMCTX_EXTRN_ALL_MSRS | CPUMCTX_EXTRN_CR0
                                                     | CPUMCTX_EXTRN_CR3 | CPUMCTX_EXTRN_CR4,
                                                     "MSRs");
        if (rcStrict == VINF_SUCCESS)
        {
            /*
             * Handle writes.
             */
            if (pExit->MsrAccess.AccessInfo.IsWrite)
            {
                rcStrict = CPUMSetGuestMsr(pVCpu, pExit->MsrAccess.MsrNumber,
                                           RT_MAKE_U64((uint32_t)pExit->MsrAccess.Rax, (uint32_t)pExit->MsrAccess.Rdx));
                Log4(("MsrExit/%u: %04x:%08RX64/%s: WRMSR %08x, %08x:%08x -> %Rrc\n", pVCpu->idCpu, pExit->VpContext.Cs.Selector,
                      pExit->VpContext.Rip, nemR3WinExecStateToLogStr(&pExit->VpContext), pExit->MsrAccess.MsrNumber,
                      (uint32_t)pExit->MsrAccess.Rax, (uint32_t)pExit->MsrAccess.Rdx, VBOXSTRICTRC_VAL(rcStrict) ));
                if (rcStrict == VINF_SUCCESS)
                {
                    nemR3WinAdvanceGuestRipAndClearRF(pVCpu, pCtx, &pExit->VpContext);
                    return VINF_SUCCESS;
                }
                LogRel(("MsrExit/%u: %04x:%08RX64/%s: WRMSR %08x, %08x:%08x -> %Rrc!\n", pVCpu->idCpu,
                        pExit->VpContext.Cs.Selector, pExit->VpContext.Rip, nemR3WinExecStateToLogStr(&pExit->VpContext),
                        pExit->MsrAccess.MsrNumber, (uint32_t)pExit->MsrAccess.Rax, (uint32_t)pExit->MsrAccess.Rdx,
                        VBOXSTRICTRC_VAL(rcStrict) ));
            }
            /*
             * Handle reads.
             */
            else
            {
                uint64_t uValue = 0;
                rcStrict = CPUMQueryGuestMsr(pVCpu, pExit->MsrAccess.MsrNumber, &uValue);
                Log4(("MsrExit/%u: %04x:%08RX64/%s: RDMSR %08x -> %08RX64 / %Rrc\n", pVCpu->idCpu,
                      pExit->VpContext.Cs.Selector, pExit->VpContext.Rip, nemR3WinExecStateToLogStr(&pExit->VpContext),
                      pExit->MsrAccess.MsrNumber, uValue, VBOXSTRICTRC_VAL(rcStrict) ));
                if (rcStrict == VINF_SUCCESS)
                {
                    pCtx->rax = (uint32_t)uValue;
                    pCtx->rdx = uValue >> 32;
                    pCtx->fExtrn &= ~(CPUMCTX_EXTRN_RAX | CPUMCTX_EXTRN_RDX);
                    nemR3WinAdvanceGuestRipAndClearRF(pVCpu, pCtx, &pExit->VpContext);
                    return VINF_SUCCESS;
                }
                LogRel(("MsrExit/%u: %04x:%08RX64/%s: RDMSR %08x -> %08RX64 / %Rrc\n", pVCpu->idCpu, pExit->VpContext.Cs.Selector,
                        pExit->VpContext.Rip, nemR3WinExecStateToLogStr(&pExit->VpContext), pExit->MsrAccess.MsrNumber,
                        uValue, VBOXSTRICTRC_VAL(rcStrict) ));
            }
        }
        else
        {
            LogRel(("MsrExit/%u: %04x:%08RX64/%s: %sMSR %08x -> %Rrc - msr state import\n",
                    pVCpu->idCpu, pExit->VpContext.Cs.Selector, pExit->VpContext.Rip, nemR3WinExecStateToLogStr(&pExit->VpContext),
                    pExit->MsrAccess.AccessInfo.IsWrite ? "WR" : "RD", pExit->MsrAccess.MsrNumber, VBOXSTRICTRC_VAL(rcStrict) ));
            return rcStrict;
        }
    }
    else if (pExit->MsrAccess.AccessInfo.IsWrite)
        Log4(("MsrExit/%u: %04x:%08RX64/%s: CPL %u -> #GP(0); WRMSR %08x, %08x:%08x\n", pVCpu->idCpu, pExit->VpContext.Cs.Selector,
              pExit->VpContext.Rip, nemR3WinExecStateToLogStr(&pExit->VpContext), pExit->VpContext.ExecutionState.Cpl,
              pExit->MsrAccess.MsrNumber, (uint32_t)pExit->MsrAccess.Rax, (uint32_t)pExit->MsrAccess.Rdx ));
    else
        Log4(("MsrExit/%u: %04x:%08RX64/%s: CPL %u -> #GP(0); RDMSR %08x\n", pVCpu->idCpu, pExit->VpContext.Cs.Selector,
              pExit->VpContext.Rip, nemR3WinExecStateToLogStr(&pExit->VpContext), pExit->VpContext.ExecutionState.Cpl,
              pExit->MsrAccess.MsrNumber));

    /*
     * If we get down here, we're supposed to #GP(0).
     */
    rcStrict = nemHCWinImportStateIfNeededStrict(pVCpu, NULL, pCtx, NEM_WIN_CPUMCTX_EXTRN_MASK_FOR_IEM, "MSR");
    if (rcStrict == VINF_SUCCESS)
    {
        rcStrict = IEMInjectTrap(pVCpu, X86_XCPT_GP, TRPM_TRAP, 0, 0, 0);
        if (rcStrict == VINF_IEM_RAISED_XCPT)
            rcStrict = VINF_SUCCESS;
        else if (rcStrict != VINF_SUCCESS)
            Log4(("MsrExit/%u: Injecting #GP(0) failed: %Rrc\n", VBOXSTRICTRC_VAL(rcStrict) ));
    }

    RT_NOREF_PV(pVM);
    return rcStrict;
}
#endif /* IN_RING3 && !NEM_WIN_USE_OUR_OWN_RUN_API */


/**
 * Worker for nemHCWinHandleMessageException & nemR3WinHandleExitException that
 * checks if the given opcodes are of interest at all.
 *
 * @returns true if interesting, false if not.
 * @param   cbOpcodes           Number of opcode bytes available.
 * @param   pbOpcodes           The opcode bytes.
 * @param   f64BitMode          Whether we're in 64-bit mode.
 */
DECLINLINE(bool) nemHcWinIsInterestingUndefinedOpcode(uint8_t cbOpcodes, uint8_t const *pbOpcodes, bool f64BitMode)
{
    /*
     * Currently only interested in VMCALL and VMMCALL.
     */
    while (cbOpcodes >= 3)
    {
        switch (pbOpcodes[0])
        {
            case 0x0f:
                switch (pbOpcodes[1])
                {
                    case 0x01:
                        switch (pbOpcodes[2])
                        {
                            case 0xc1: /* 0f 01 c1  VMCALL */
                                return true;
                            case 0xd9: /* 0f 01 d9  VMMCALL */
                                return true;
                            default:
                                break;
                        }
                        break;
                }
                break;

            default:
                return false;

            /* prefixes */
            case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
            case 0x48: case 0x49: case 0x4a: case 0x4b: case 0x4c: case 0x4d: case 0x4e: case 0x4f:
                if (!f64BitMode)
                    return false;
                RT_FALL_THRU();
            case X86_OP_PRF_CS:
            case X86_OP_PRF_SS:
            case X86_OP_PRF_DS:
            case X86_OP_PRF_ES:
            case X86_OP_PRF_FS:
            case X86_OP_PRF_GS:
            case X86_OP_PRF_SIZE_OP:
            case X86_OP_PRF_SIZE_ADDR:
            case X86_OP_PRF_LOCK:
            case X86_OP_PRF_REPZ:
            case X86_OP_PRF_REPNZ:
                cbOpcodes--;
                pbOpcodes++;
                continue;
        }
        break;
    }
    return false;
}


#ifdef NEM_WIN_USE_OUR_OWN_RUN_API
/**
 * Copies state included in a exception intercept message.
 *
 * @param   pVCpu           The cross context per CPU structure.
 * @param   pMsg            The message.
 * @param   pCtx            The register context.
 * @param   fClearXcpt      Clear pending exception.
 */
DECLINLINE(void) nemHCWinCopyStateFromExceptionMessage(PVMCPU pVCpu, HV_X64_EXCEPTION_INTERCEPT_MESSAGE const *pMsg,
                                                       PCPUMCTX pCtx, bool fClearXcpt)
{
    nemHCWinCopyStateFromX64Header(pVCpu, pCtx, &pMsg->Header);
    pCtx->fExtrn &= ~(  CPUMCTX_EXTRN_GPRS_MASK | CPUMCTX_EXTRN_SS | CPUMCTX_EXTRN_DS
                      | (fClearXcpt ? CPUMCTX_EXTRN_NEM_WIN_EVENT_INJECT : 0) );
    pCtx->rax = pMsg->Rax;
    pCtx->rcx = pMsg->Rcx;
    pCtx->rdx = pMsg->Rdx;
    pCtx->rbx = pMsg->Rbx;
    pCtx->rsp = pMsg->Rsp;
    pCtx->rbp = pMsg->Rbp;
    pCtx->rsi = pMsg->Rsi;
    pCtx->rdi = pMsg->Rdi;
    pCtx->r8  = pMsg->R8;
    pCtx->r9  = pMsg->R9;
    pCtx->r10 = pMsg->R10;
    pCtx->r11 = pMsg->R11;
    pCtx->r12 = pMsg->R12;
    pCtx->r13 = pMsg->R13;
    pCtx->r14 = pMsg->R14;
    pCtx->r15 = pMsg->R15;
    NEM_WIN_COPY_BACK_SEG(pCtx->ds, pMsg->DsSegment);
    NEM_WIN_COPY_BACK_SEG(pCtx->ss, pMsg->SsSegment);
}
#elif defined(IN_RING3)
/**
 * Copies state included in a exception intercept exit.
 *
 * @param   pVCpu           The cross context per CPU structure.
 * @param   pExit           The VM exit information.
 * @param   pCtx            The register context.
 * @param   fClearXcpt      Clear pending exception.
 */
DECLINLINE(void) nemR3WinCopyStateFromExceptionMessage(PVMCPU pVCpu, WHV_RUN_VP_EXIT_CONTEXT const *pExit,
                                                       PCPUMCTX pCtx, bool fClearXcpt)
{
    nemR3WinCopyStateFromX64Header(pVCpu, pCtx, &pExit->VpContext);
    if (fClearXcpt)
        pCtx->fExtrn &= ~CPUMCTX_EXTRN_NEM_WIN_EVENT_INJECT;
}
#endif /* IN_RING3 && !NEM_WIN_USE_OUR_OWN_RUN_API */


#ifdef NEM_WIN_USE_OUR_OWN_RUN_API
/**
 * Deals with exception intercept message (HvMessageTypeX64ExceptionIntercept).
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context per CPU structure.
 * @param   pMsg            The message.
 * @param   pCtx            The register context.
 * @param   pGVCpu          The global (ring-0) per CPU structure (NULL in r3).
 * @sa      nemR3WinHandleExitMsr
 */
NEM_TMPL_STATIC VBOXSTRICTRC
nemHCWinHandleMessageException(PVMCPU pVCpu, HV_X64_EXCEPTION_INTERCEPT_MESSAGE const *pMsg, PCPUMCTX pCtx, PGVMCPU pGVCpu)
{
    /*
     * Assert sanity.
     */
    AssertMsg(pMsg->Header.InstructionLength < 0x10, ("%#x\n", pMsg->Header.InstructionLength));
    Assert(   pMsg->Header.InterceptAccessType == HV_INTERCEPT_ACCESS_READ
           || pMsg->Header.InterceptAccessType == HV_INTERCEPT_ACCESS_WRITE
           || pMsg->Header.InterceptAccessType == HV_INTERCEPT_ACCESS_EXECUTE);

    /*
     * Get most of the register state since we'll end up making IEM inject the
     * event.  The exception isn't normally flaged as a pending event, so duh.
     *
     * Note! We can optimize this later with event injection.
     */
    Log4(("XcptExit/%u: %04x:%08RX64/%s: %x errcd=%#x parm=%RX64\n",
          pVCpu->idCpu, pMsg->Header.CsSegment.Selector, pMsg->Header.Rip, nemHCWinExecStateToLogStr(&pMsg->Header),
          pMsg->ExceptionVector, pMsg->ErrorCode, pMsg->ExceptionParameter));
    nemHCWinCopyStateFromExceptionMessage(pVCpu, pMsg, pCtx, true /*fClearXcpt*/);
    VBOXSTRICTRC rcStrict = nemHCWinImportStateIfNeededStrict(pVCpu, pGVCpu, pCtx, NEM_WIN_CPUMCTX_EXTRN_MASK_FOR_IEM, "Xcpt");
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    /*
     * Handle the intercept.
     */
    switch (pMsg->ExceptionVector)
    {
        /*
         * We get undefined opcodes on VMMCALL(AMD) & VMCALL(Intel) instructions
         * and need to turn them over to GIM.
         */
        case X86_XCPT_UD:
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitExceptionUd);
            /** @todo Call GIMXcptUD if required. */
            if (nemHcWinIsInterestingUndefinedOpcode(pMsg->InstructionByteCount, pMsg->InstructionBytes,
                                                     pMsg->Header.ExecutionState.EferLma && pMsg->Header.CsSegment.Long ))
            {
                rcStrict = IEMExecOneWithPrefetchedByPC(pVCpu, CPUMCTX2CORE(pCtx), pMsg->Header.Rip, pMsg->InstructionBytes,
                                                        pMsg->InstructionByteCount);
                Log4(("XcptExit/%u: %04x:%08RX64/%s: #UD -> emulated -> %Rrc\n",
                      pVCpu->idCpu, pMsg->Header.CsSegment.Selector, pMsg->Header.Rip,
                      nemHCWinExecStateToLogStr(&pMsg->Header), VBOXSTRICTRC_VAL(rcStrict) ));
                STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitExceptionUdHandled);
                return rcStrict;
            }
            Log4(("XcptExit/%u: %04x:%08RX64/%s: #UD [%.*Rhxs] -> re-injected\n", pVCpu->idCpu, pMsg->Header.CsSegment.Selector,
                  pMsg->Header.Rip, nemHCWinExecStateToLogStr(&pMsg->Header),  pMsg->InstructionByteCount, pMsg->InstructionBytes ));
            break;

        /*
         * Filter debug exceptions.
         */
        case X86_XCPT_DB:
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitExceptionDb);
            Log4(("XcptExit/%u: %04x:%08RX64/%s: #DB - TODO\n",
                  pVCpu->idCpu, pMsg->Header.CsSegment.Selector, pMsg->Header.Rip, nemHCWinExecStateToLogStr(&pMsg->Header) ));
            break;

        case X86_XCPT_BP:
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitExceptionBp);
            Log4(("XcptExit/%u: %04x:%08RX64/%s: #BP - TODO\n",
                  pVCpu->idCpu, pMsg->Header.CsSegment.Selector, pMsg->Header.Rip, nemHCWinExecStateToLogStr(&pMsg->Header) ));
            break;

        /* This shouldn't happen. */
        default:
            AssertLogRelMsgFailedReturn(("ExceptionVector=%#x\n", pMsg->ExceptionVector),  VERR_IEM_IPE_6);
    }

    /*
     * Inject it.
     */
    rcStrict = IEMInjectTrap(pVCpu, pMsg->ExceptionVector, TRPM_TRAP, pMsg->ErrorCode,
                             pMsg->ExceptionParameter /*??*/, pMsg->Header.InstructionLength);
    Log4(("XcptExit/%u: %04x:%08RX64/%s: %#u -> injected -> %Rrc\n",
          pVCpu->idCpu, pMsg->Header.CsSegment.Selector, pMsg->Header.Rip,
          nemHCWinExecStateToLogStr(&pMsg->Header), pMsg->ExceptionVector, VBOXSTRICTRC_VAL(rcStrict) ));
    return rcStrict;
}
#elif defined(IN_RING3)
/**
 * Deals with MSR access exits (WHvRunVpExitReasonException).
 *
 * @returns Strict VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context per CPU structure.
 * @param   pExit           The VM exit information to handle.
 * @param   pCtx            The register context.
 * @sa      nemR3WinHandleExitException
 */
NEM_TMPL_STATIC VBOXSTRICTRC
nemR3WinHandleExitException(PVM pVM, PVMCPU pVCpu, WHV_RUN_VP_EXIT_CONTEXT const *pExit, PCPUMCTX pCtx)
{
    /*
     * Assert sanity.
     */
    AssertMsg(pExit->VpContext.InstructionLength < 0x10, ("%#x\n", pExit->VpContext.InstructionLength));

    /*
     * Get most of the register state since we'll end up making IEM inject the
     * event.  The exception isn't normally flaged as a pending event, so duh.
     *
     * Note! We can optimize this later with event injection.
     */
    Log4(("XcptExit/%u: %04x:%08RX64/%s: %x errcd=%#x parm=%RX64\n", pVCpu->idCpu, pExit->VpContext.Cs.Selector,
          pExit->VpContext.Rip, nemR3WinExecStateToLogStr(&pExit->VpContext), pExit->VpException.ExceptionType,
          pExit->VpException.ErrorCode, pExit->VpException.ExceptionParameter ));
    nemR3WinCopyStateFromExceptionMessage(pVCpu, pExit, pCtx, true /*fClearXcpt*/);
    VBOXSTRICTRC rcStrict = nemHCWinImportStateIfNeededStrict(pVCpu, NULL, pCtx, NEM_WIN_CPUMCTX_EXTRN_MASK_FOR_IEM, "Xcpt");
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    /*
     * Handle the intercept.
     */
    switch (pExit->VpException.ExceptionType)
    {
        /*
         * We get undefined opcodes on VMMCALL(AMD) & VMCALL(Intel) instructions
         * and need to turn them over to GIM.
         */
        case X86_XCPT_UD:
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitExceptionUd);
            /** @todo Call GIMXcptUD if required. */
            if (nemHcWinIsInterestingUndefinedOpcode(pExit->VpException.InstructionByteCount, pExit->VpException.InstructionBytes,
                                                     pExit->VpContext.ExecutionState.EferLma && pExit->VpContext.Cs.Long ))
            {
                rcStrict = IEMExecOneWithPrefetchedByPC(pVCpu, CPUMCTX2CORE(pCtx), pExit->VpContext.Rip,
                                                        pExit->VpException.InstructionBytes,
                                                        pExit->VpException.InstructionByteCount);
                Log4(("XcptExit/%u: %04x:%08RX64/%s: #UD -> emulated -> %Rrc\n",
                      pVCpu->idCpu, pExit->VpContext.Cs.Selector, pExit->VpContext.Rip,
                      nemR3WinExecStateToLogStr(&pExit->VpContext), VBOXSTRICTRC_VAL(rcStrict) ));
                STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitExceptionUdHandled);
                return rcStrict;
            }

            Log4(("XcptExit/%u: %04x:%08RX64/%s: #UD [%.*Rhxs] -> re-injected\n", pVCpu->idCpu,
                  pExit->VpContext.Cs.Selector, pExit->VpContext.Rip, nemR3WinExecStateToLogStr(&pExit->VpContext),
                  pExit->VpException.InstructionByteCount, pExit->VpException.InstructionBytes ));
            break;

        /*
         * Filter debug exceptions.
         */
        case X86_XCPT_DB:
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitExceptionDb);
            Log4(("XcptExit/%u: %04x:%08RX64/%s: #DB - TODO\n",
                  pVCpu->idCpu, pExit->VpContext.Cs.Selector, pExit->VpContext.Rip, nemR3WinExecStateToLogStr(&pExit->VpContext) ));
            break;

        case X86_XCPT_BP:
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitExceptionBp);
            Log4(("XcptExit/%u: %04x:%08RX64/%s: #BP - TODO\n",
                  pVCpu->idCpu, pExit->VpContext.Cs.Selector, pExit->VpContext.Rip, nemR3WinExecStateToLogStr(&pExit->VpContext) ));
            break;

        /* This shouldn't happen. */
        default:
            AssertLogRelMsgFailedReturn(("ExceptionType=%#x\n", pExit->VpException.ExceptionType),  VERR_IEM_IPE_6);
    }

    /*
     * Inject it.
     */
    rcStrict = IEMInjectTrap(pVCpu, pExit->VpException.ExceptionType, TRPM_TRAP, pExit->VpException.ErrorCode,
                             pExit->VpException.ExceptionParameter /*??*/, pExit->VpContext.InstructionLength);
    Log4(("XcptExit/%u: %04x:%08RX64/%s: %#u -> injected -> %Rrc\n",
          pVCpu->idCpu, pExit->VpContext.Cs.Selector, pExit->VpContext.Rip,
          nemR3WinExecStateToLogStr(&pExit->VpContext), pExit->VpException.ExceptionType, VBOXSTRICTRC_VAL(rcStrict) ));

    RT_NOREF_PV(pVM);
    return rcStrict;
}
#endif /* IN_RING3 && !NEM_WIN_USE_OUR_OWN_RUN_API */


#ifdef NEM_WIN_USE_OUR_OWN_RUN_API
/**
 * Deals with unrecoverable exception (triple fault).
 *
 * Seen WRMSR 0x201 (IA32_MTRR_PHYSMASK0) writes from grub / debian9 ending up
 * here too.  So we'll leave it to IEM to decide.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context per CPU structure.
 * @param   pMsgHdr         The message header.
 * @param   pCtx            The register context.
 * @param   pGVCpu          The global (ring-0) per CPU structure (NULL in r3).
 * @sa      nemR3WinHandleExitUnrecoverableException
 */
NEM_TMPL_STATIC VBOXSTRICTRC nemHCWinHandleMessageUnrecoverableException(PVMCPU pVCpu,
                                                                         HV_X64_INTERCEPT_MESSAGE_HEADER const *pMsgHdr,
                                                                         PCPUMCTX pCtx, PGVMCPU pGVCpu)
{
    AssertMsg(pMsgHdr->InstructionLength < 0x10, ("%#x\n", pMsgHdr->InstructionLength));

# if 0
    /*
     * Just copy the state we've got and handle it in the loop for now.
     */
    nemHCWinCopyStateFromX64Header(pVCpu, pCtx, pMsgHdr);
    Log(("TripleExit/%u: %04x:%08RX64/%s: RFL=%#RX64 -> VINF_EM_TRIPLE_FAULT\n",
         pVCpu->idCpu, pMsgHdr->CsSegment.Selector, pMsgHdr->Rip, nemHCWinExecStateToLogStr(&pMsg->Header), pMsgHdr->Rflags));
    return VINF_EM_TRIPLE_FAULT;
# else
    /*
     * Let IEM decide whether this is really it.
     */
    nemHCWinCopyStateFromX64Header(pVCpu, pCtx, pMsgHdr);
    VBOXSTRICTRC rcStrict = nemHCWinImportStateIfNeededStrict(pVCpu, pGVCpu, pCtx,
                                                              NEM_WIN_CPUMCTX_EXTRN_MASK_FOR_IEM | CPUMCTX_EXTRN_ALL, "TripleExit");
    if (rcStrict == VINF_SUCCESS)
    {
        rcStrict = IEMExecOne(pVCpu);
        if (rcStrict == VINF_SUCCESS)
        {
            Log(("UnrecovExit/%u: %04x:%08RX64/%s: RFL=%#RX64 -> VINF_SUCCESS\n", pVCpu->idCpu, pMsgHdr->CsSegment.Selector,
                 pMsgHdr->Rip, nemHCWinExecStateToLogStr(pMsgHdr), pMsgHdr->Rflags ));
            pCtx->fExtrn &= ~CPUMCTX_EXTRN_NEM_WIN_EVENT_INJECT; /* Make sure to reset pending #DB(0). */
            return VINF_SUCCESS;
        }
        if (rcStrict == VINF_EM_TRIPLE_FAULT)
            Log(("UnrecovExit/%u: %04x:%08RX64/%s: RFL=%#RX64 -> VINF_EM_TRIPLE_FAULT!\n", pVCpu->idCpu, pMsgHdr->CsSegment.Selector,
                 pMsgHdr->Rip, nemHCWinExecStateToLogStr(pMsgHdr), pMsgHdr->Rflags, VBOXSTRICTRC_VAL(rcStrict) ));
        else
            Log(("UnrecovExit/%u: %04x:%08RX64/%s: RFL=%#RX64 -> %Rrc (IEMExecOne)\n", pVCpu->idCpu, pMsgHdr->CsSegment.Selector,
                 pMsgHdr->Rip, nemHCWinExecStateToLogStr(pMsgHdr), pMsgHdr->Rflags, VBOXSTRICTRC_VAL(rcStrict) ));
    }
    else
        Log(("UnrecovExit/%u: %04x:%08RX64/%s: RFL=%#RX64 -> %Rrc (state import)\n", pVCpu->idCpu, pMsgHdr->CsSegment.Selector,
             pMsgHdr->Rip, nemHCWinExecStateToLogStr(pMsgHdr), pMsgHdr->Rflags, VBOXSTRICTRC_VAL(rcStrict) ));
    return rcStrict;
# endif
}
#elif defined(IN_RING3)
/**
 * Deals with MSR access exits (WHvRunVpExitReasonUnrecoverableException).
 *
 * @returns Strict VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context per CPU structure.
 * @param   pExit           The VM exit information to handle.
 * @param   pCtx            The register context.
 * @sa      nemHCWinHandleMessageUnrecoverableException
 */
NEM_TMPL_STATIC VBOXSTRICTRC
nemR3WinHandleExitUnrecoverableException(PVM pVM, PVMCPU pVCpu, WHV_RUN_VP_EXIT_CONTEXT const *pExit, PCPUMCTX pCtx)
{
    AssertMsg(pExit->VpContext.InstructionLength < 0x10, ("%#x\n", pExit->VpContext.InstructionLength));

# if 0
    /*
     * Just copy the state we've got and handle it in the loop for now.
     */
    nemR3WinCopyStateFromX64Header(pVCpu, pCtx, &pExit->VpContext);
    Log(("TripleExit/%u: %04x:%08RX64/%s: RFL=%#RX64 -> VINF_EM_TRIPLE_FAULT\n", pVCpu->idCpu, pExit->VpContext.Cs.Selector,
         pExit->VpContext.Rip, nemR3WinExecStateToLogStr(&pExit->VpContext), pExit->VpContext.Rflags));
    RT_NOREF_PV(pVM);
    return VINF_EM_TRIPLE_FAULT;
# else
    /*
     * Let IEM decide whether this is really it.
     */
    nemR3WinCopyStateFromX64Header(pVCpu, pCtx, &pExit->VpContext);
    VBOXSTRICTRC rcStrict = nemHCWinImportStateIfNeededStrict(pVCpu, NULL, pCtx,
                                                              NEM_WIN_CPUMCTX_EXTRN_MASK_FOR_IEM | CPUMCTX_EXTRN_ALL, "TripleExit");
    if (rcStrict == VINF_SUCCESS)
    {
        rcStrict = IEMExecOne(pVCpu);
        if (rcStrict == VINF_SUCCESS)
        {
            Log(("UnrecovExit/%u: %04x:%08RX64/%s: RFL=%#RX64 -> VINF_SUCCESS\n", pVCpu->idCpu, pExit->VpContext.Cs.Selector,
                 pExit->VpContext.Rip, nemR3WinExecStateToLogStr(&pExit->VpContext), pExit->VpContext.Rflags));
            pCtx->fExtrn &= ~CPUMCTX_EXTRN_NEM_WIN_EVENT_INJECT; /* Make sure to reset pending #DB(0). */
            return VINF_SUCCESS;
        }
        if (rcStrict == VINF_EM_TRIPLE_FAULT)
            Log(("UnrecovExit/%u: %04x:%08RX64/%s: RFL=%#RX64 -> VINF_EM_TRIPLE_FAULT!\n", pVCpu->idCpu, pExit->VpContext.Cs.Selector,
                 pExit->VpContext.Rip, nemR3WinExecStateToLogStr(&pExit->VpContext), pExit->VpContext.Rflags, VBOXSTRICTRC_VAL(rcStrict) ));
        else
            Log(("UnrecovExit/%u: %04x:%08RX64/%s: RFL=%#RX64 -> %Rrc (IEMExecOne)\n", pVCpu->idCpu, pExit->VpContext.Cs.Selector,
                 pExit->VpContext.Rip, nemR3WinExecStateToLogStr(&pExit->VpContext), pExit->VpContext.Rflags, VBOXSTRICTRC_VAL(rcStrict) ));
    }
    else
        Log(("UnrecovExit/%u: %04x:%08RX64/%s: RFL=%#RX64 -> %Rrc (state import)\n", pVCpu->idCpu, pExit->VpContext.Cs.Selector,
             pExit->VpContext.Rip, nemR3WinExecStateToLogStr(&pExit->VpContext), pExit->VpContext.Rflags, VBOXSTRICTRC_VAL(rcStrict) ));
    RT_NOREF_PV(pVM);
    return rcStrict;
# endif

}
#endif /* IN_RING3 && !NEM_WIN_USE_OUR_OWN_RUN_API */

#ifdef NEM_WIN_USE_OUR_OWN_RUN_API
/**
 * Handles messages (VM exits).
 *
 * @returns Strict VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context per CPU structure.
 * @param   pMappingHeader  The message slot mapping.
 * @param   pCtx            The register context.
 * @param   pGVCpu          The global (ring-0) per CPU structure (NULL in r3).
 * @sa      nemR3WinHandleExit
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
                Log4(("HaltExit\n"));
                return VINF_EM_HALT;

            case HvMessageTypeX64InterruptWindow:
                Assert(pMsg->Header.PayloadSize == sizeof(pMsg->X64InterruptWindow));
                STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitInterruptWindow);
                return nemHCWinHandleMessageInterruptWindow(pVM, pVCpu, &pMsg->X64InterruptWindow, pCtx, pGVCpu);

            case HvMessageTypeX64CpuidIntercept:
                Assert(pMsg->Header.PayloadSize == sizeof(pMsg->X64CpuIdIntercept));
                STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitCpuId);
                return nemHCWinHandleMessageCpuId(pVCpu, &pMsg->X64CpuIdIntercept, pCtx);

            case HvMessageTypeX64MsrIntercept:
                Assert(pMsg->Header.PayloadSize == sizeof(pMsg->X64MsrIntercept));
                STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitMsr);
                return nemHCWinHandleMessageMsr(pVCpu, &pMsg->X64MsrIntercept, pCtx, pGVCpu);

            case HvMessageTypeX64ExceptionIntercept:
                Assert(pMsg->Header.PayloadSize == sizeof(pMsg->X64ExceptionIntercept));
                STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitException);
                return nemHCWinHandleMessageException(pVCpu, &pMsg->X64ExceptionIntercept, pCtx, pGVCpu);

            case HvMessageTypeUnrecoverableException:
                Assert(pMsg->Header.PayloadSize == sizeof(pMsg->X64InterceptHeader));
                STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitUnrecoverable);
                return nemHCWinHandleMessageUnrecoverableException(pVCpu, &pMsg->X64InterceptHeader, pCtx, pGVCpu);

            case HvMessageTypeInvalidVpRegisterValue:
            case HvMessageTypeUnsupportedFeature:
            case HvMessageTypeTlbPageSizeMismatch:
                LogRel(("Unimplemented msg:\n%.*Rhxd\n", (int)sizeof(*pMsg), pMsg));
                AssertLogRelMsgFailedReturn(("Message type %#x not implemented!\n%.32Rhxd\n", pMsg->Header.MessageType, pMsg),
                                            VERR_NEM_IPE_3);

            case HvMessageTypeX64ApicEoi:
            case HvMessageTypeX64LegacyFpError:
            case HvMessageTypeX64RegisterIntercept:
            case HvMessageTypeApicEoi:
            case HvMessageTypeFerrAsserted:
            case HvMessageTypeEventLogBufferComplete:
            case HvMessageTimerExpired:
                LogRel(("Unexpected msg:\n%.*Rhxd\n", (int)sizeof(*pMsg), pMsg));
                AssertLogRelMsgFailedReturn(("Unexpected message on CPU #%u: %#x\n", pVCpu->idCpu, pMsg->Header.MessageType),
                                            VERR_NEM_IPE_3);

            default:
                LogRel(("Unknown msg:\n%.*Rhxd\n", (int)sizeof(*pMsg), pMsg));
                AssertLogRelMsgFailedReturn(("Unknown message on CPU #%u: %#x\n", pVCpu->idCpu, pMsg->Header.MessageType),
                                            VERR_NEM_IPE_3);
        }
    }
    else
        AssertLogRelMsgFailedReturn(("Unexpected VID message type on CPU #%u: %#x LB %u\n",
                                     pVCpu->idCpu, pMappingHeader->enmVidMsgType, pMappingHeader->cbMessage),
                                    VERR_NEM_IPE_4);
}
#elif defined(IN_RING3)
/**
 * Handles VM exits.
 *
 * @returns Strict VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context per CPU structure.
 * @param   pExit           The VM exit information to handle.
 * @param   pCtx            The register context.
 * @sa      nemHCWinHandleMessage
 */
NEM_TMPL_STATIC VBOXSTRICTRC nemR3WinHandleExit(PVM pVM, PVMCPU pVCpu, WHV_RUN_VP_EXIT_CONTEXT const *pExit, PCPUMCTX pCtx)
{
    switch (pExit->ExitReason)
    {
        case WHvRunVpExitReasonMemoryAccess:
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitMemUnmapped);
            return nemR3WinHandleExitMemory(pVM, pVCpu, pExit, pCtx);

        case WHvRunVpExitReasonX64IoPortAccess:
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitPortIo);
            return nemR3WinHandleExitIoPort(pVM, pVCpu, pExit, pCtx);

        case WHvRunVpExitReasonX64Halt:
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitHalt);
            Log4(("HaltExit\n"));
            return VINF_EM_HALT;

        case WHvRunVpExitReasonCanceled:
            return VINF_SUCCESS;

        case WHvRunVpExitReasonX64InterruptWindow:
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitInterruptWindow);
            return nemR3WinHandleExitInterruptWindow(pVM, pVCpu, pExit, pCtx);

        case WHvRunVpExitReasonX64Cpuid:
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitCpuId);
            return nemR3WinHandleExitCpuId(pVM, pVCpu, pExit, pCtx);

        case WHvRunVpExitReasonX64MsrAccess:
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitMsr);
            return nemR3WinHandleExitMsr(pVM, pVCpu, pExit, pCtx);

        case WHvRunVpExitReasonException:
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitException);
            return nemR3WinHandleExitException(pVM, pVCpu, pExit, pCtx);

        case WHvRunVpExitReasonUnrecoverableException:
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitUnrecoverable);
            return nemR3WinHandleExitUnrecoverableException(pVM, pVCpu, pExit, pCtx);

        case WHvRunVpExitReasonUnsupportedFeature:
        case WHvRunVpExitReasonInvalidVpRegisterValue:
            LogRel(("Unimplemented exit:\n%.*Rhxd\n", (int)sizeof(*pExit), pExit));
            AssertLogRelMsgFailedReturn(("Unexpected exit on CPU #%u: %#x\n%.32Rhxd\n",
                                         pVCpu->idCpu, pExit->ExitReason, pExit), VERR_NEM_IPE_3);

        /* Undesired exits: */
        case WHvRunVpExitReasonNone:
        default:
            LogRel(("Unknown exit:\n%.*Rhxd\n", (int)sizeof(*pExit), pExit));
            AssertLogRelMsgFailedReturn(("Unknown exit on CPU #%u: %#x!\n", pVCpu->idCpu, pExit->ExitReason), VERR_NEM_IPE_3);
    }
}
#endif /* IN_RING3 && !NEM_WIN_USE_OUR_OWN_RUN_API */

#ifdef NEM_WIN_USE_OUR_OWN_RUN_API
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
                          RT_SUCCESS(rcStrict) ?  VERR_NEM_IPE_5 : rcStrict);
# else
    DWORD dwErr = RTNtLastErrorValue();
    AssertLogRelMsgReturn(dwErr == ERROR_VID_STOP_PENDING, ("dwErr=%#u (%#x)\n", dwErr, dwErr),
                          RT_SUCCESS(rcStrict) ?  VERR_NEM_IPE_5 : rcStrict);
# endif
    Log8(("nemHCWinStopCpu: Stopping CPU #%u pending...\n", pVCpu->idCpu));
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
                          RT_SUCCESS(rcStrict) ? VERR_NEM_IPE_5 : rcStrict);
# else
    BOOL fWait = g_pfnVidMessageSlotHandleAndGetNext(pVM->nem.s.hPartitionDevice, pVCpu->idCpu,
                                                     VID_MSHAGN_F_GET_NEXT_MESSAGE, 30000 /*ms*/);
    AssertLogRelMsgReturn(fWait, ("1st VidMessageSlotHandleAndGetNext after ERROR_VID_STOP_PENDING failed: %u\n", RTNtLastErrorValue()),
                          RT_SUCCESS(rcStrict) ? VERR_NEM_IPE_5 : rcStrict);
# endif

    /* It should be a hypervisor message and definitely not a stop request completed message. */
    VID_MESSAGE_TYPE enmVidMsgType = pMappingHeader->enmVidMsgType;
    AssertLogRelMsgReturn(enmVidMsgType != VidMessageStopRequestComplete,
                          ("Unexpected 1st message following ERROR_VID_STOP_PENDING: %#x LB %#x\n",
                           enmVidMsgType, pMappingHeader->cbMessage),
                          RT_SUCCESS(rcStrict) ? VERR_NEM_IPE_5 : rcStrict);

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
    AssertLogRelMsgReturn(NT_SUCCESS(rcNt), ("2nd VidMessageSlotHandleAndGetNext after ERROR_VID_STOP_PENDING failed: %#x\n", rcNt),
                          RT_SUCCESS(rcStrict) ? VERR_NEM_IPE_5 : rcStrict);
# else
    fWait = g_pfnVidMessageSlotHandleAndGetNext(pVM->nem.s.hPartitionDevice, pVCpu->idCpu,
                                                VID_MSHAGN_F_HANDLE_MESSAGE | VID_MSHAGN_F_GET_NEXT_MESSAGE, 30000 /*ms*/);
    AssertLogRelMsgReturn(fWait, ("2nd VidMessageSlotHandleAndGetNext after ERROR_VID_STOP_PENDING failed: %u\n", RTNtLastErrorValue()),
                          RT_SUCCESS(rcStrict) ? VERR_NEM_IPE_5 : rcStrict);
# endif

    /* It should be a stop request completed message. */
    enmVidMsgType = pMappingHeader->enmVidMsgType;
    AssertLogRelMsgReturn(enmVidMsgType == VidMessageStopRequestComplete,
                          ("Unexpected 2nd message following ERROR_VID_STOP_PENDING: %#x LB %#x\n",
                           enmVidMsgType, pMappingHeader->cbMessage),
                          RT_SUCCESS(rcStrict) ? VERR_NEM_IPE_5 : rcStrict);

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
                          RT_SUCCESS(rcStrict) ? VERR_NEM_IPE_5 : rcStrict);
# else
    fWait = g_pfnVidMessageSlotHandleAndGetNext(pVM->nem.s.hPartitionDevice, pVCpu->idCpu, VID_MSHAGN_F_HANDLE_MESSAGE, 30000 /*ms*/);
    AssertLogRelMsgReturn(fWait, ("3rd VidMessageSlotHandleAndGetNext after ERROR_VID_STOP_PENDING failed: %u\n", RTNtLastErrorValue()),
                          RT_SUCCESS(rcStrict) ? VERR_NEM_IPE_5 : rcStrict);
# endif
    Log8(("nemHCWinStopCpu: Stopped the CPU (rcStrict=%Rrc)\n", VBOXSTRICTRC_VAL(rcStrict) ));
    return rcStrict;
}
#endif /* NEM_WIN_USE_OUR_OWN_RUN_API */

#if defined(NEM_WIN_USE_OUR_OWN_RUN_API) || defined(IN_RING3)

/**
 * Deals with pending interrupt related force flags, may inject interrupt.
 *
 * @returns VBox strict status code.
 * @param   pVM                 The cross context VM structure.
 * @param   pVCpu               The cross context per CPU structure.
 * @param   pGVCpu              The global (ring-0) per CPU structure.
 * @param   pCtx                The register context.
 * @param   pfInterruptWindows  Where to return interrupt window flags.
 */
NEM_TMPL_STATIC VBOXSTRICTRC
nemHCWinHandleInterruptFF(PVM pVM, PVMCPU pVCpu, PGVMCPU pGVCpu, PCPUMCTX pCtx, uint8_t *pfInterruptWindows)
{
    Assert(!TRPMHasTrap(pVCpu));
    RT_NOREF_PV(pVM);

    /*
     * First update APIC.
     */
    if (VMCPU_FF_TEST_AND_CLEAR(pVCpu, VMCPU_FF_UPDATE_APIC))
    {
        APICUpdatePendingInterrupts(pVCpu);
        if (!VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INTERRUPT_APIC | VMCPU_FF_INTERRUPT_PIC
                                      | VMCPU_FF_INTERRUPT_NMI  | VMCPU_FF_INTERRUPT_SMI))
            return VINF_SUCCESS;
    }

    /*
     * We don't currently implement SMIs.
     */
    AssertReturn(!VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INTERRUPT_SMI), VERR_NEM_IPE_0);

    /*
     * Check if we've got the minimum of state required for deciding whether we
     * can inject interrupts and NMIs.  If we don't have it, get all we might require
     * for injection via IEM.
     */
    bool const fPendingNmi = VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INTERRUPT_NMI);
    uint64_t   fNeedExtrn  = CPUMCTX_EXTRN_NEM_WIN_INHIBIT_INT | CPUMCTX_EXTRN_RIP | CPUMCTX_EXTRN_RFLAGS
                           | (fPendingNmi ? CPUMCTX_EXTRN_NEM_WIN_INHIBIT_NMI : 0);
    if (pCtx->fExtrn & fNeedExtrn)
    {
        VBOXSTRICTRC rcStrict = nemHCWinImportStateIfNeededStrict(pVCpu, pGVCpu, pCtx, NEM_WIN_CPUMCTX_EXTRN_MASK_FOR_IEM, "IntFF");
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
    }
    bool const fInhibitInterrupts = VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS)
                                 && EMGetInhibitInterruptsPC(pVCpu) == pCtx->rip;

    /*
     * NMI? Try deliver it first.
     */
    if (fPendingNmi)
    {
        if (   !fInhibitInterrupts
            && !VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_BLOCK_NMIS))
        {
            VBOXSTRICTRC rcStrict = nemHCWinImportStateIfNeededStrict(pVCpu, pGVCpu, pCtx, NEM_WIN_CPUMCTX_EXTRN_MASK_FOR_IEM, "NMI");
            if (rcStrict == VINF_SUCCESS)
            {
                VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_NMI);
                rcStrict = IEMInjectTrap(pVCpu, X86_XCPT_NMI, TRPM_HARDWARE_INT, 0, 0, 0);
                Log8(("Injected NMI on %u (%d)\n", pVCpu->idCpu, VBOXSTRICTRC_VAL(rcStrict) ));
            }
            return rcStrict;
        }
        *pfInterruptWindows |= NEM_WIN_INTW_F_NMI;
        Log8(("NMI window pending on %u\n", pVCpu->idCpu));
    }

    /*
     * APIC or PIC interrupt?
     */
    if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INTERRUPT_APIC | VMCPU_FF_INTERRUPT_PIC))
    {
        if (   !fInhibitInterrupts
            && pCtx->rflags.Bits.u1IF)
        {
            VBOXSTRICTRC rcStrict = nemHCWinImportStateIfNeededStrict(pVCpu, pGVCpu, pCtx, NEM_WIN_CPUMCTX_EXTRN_MASK_FOR_IEM, "NMI");
            if (rcStrict == VINF_SUCCESS)
            {
                uint8_t bInterrupt;
                int rc = PDMGetInterrupt(pVCpu, &bInterrupt);
                if (RT_SUCCESS(rc))
                {
                    rcStrict = IEMInjectTrap(pVCpu, bInterrupt, TRPM_HARDWARE_INT, 0, 0, 0);
                    Log8(("Injected interrupt %#x on %u (%d)\n", bInterrupt, pVCpu->idCpu, VBOXSTRICTRC_VAL(rcStrict) ));
                }
                else if (rc == VERR_APIC_INTR_MASKED_BY_TPR)
                {
                    *pfInterruptWindows |= (bInterrupt >> 4 /*??*/) << NEM_WIN_INTW_F_PRIO_SHIFT;
                    Log8(("VERR_APIC_INTR_MASKED_BY_TPR: *pfInterruptWindows=%#x\n", *pfInterruptWindows));
                }
                else
                    Log8(("PDMGetInterrupt failed -> %d\n", rc));
            }
            return rcStrict;
        }
        *pfInterruptWindows |= NEM_WIN_INTW_F_REGULAR;
        Log8(("Interrupt window pending on %u\n", pVCpu->idCpu));
    }

    return VINF_SUCCESS;
}


/**
 * Inner NEM runloop for windows.
 *
 * @returns Strict VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context per CPU structure.
 * @param   pGVM            The ring-0 VM structure (NULL in ring-3).
 * @param   pGVCpu          The ring-0 per CPU structure (NULL in ring-3).
 */
NEM_TMPL_STATIC VBOXSTRICTRC nemHCWinRunGC(PVM pVM, PVMCPU pVCpu, PGVM pGVM, PGVMCPU pGVCpu)
{
    PCPUMCTX pCtx = CPUMQueryGuestCtxPtr(pVCpu);
    LogFlow(("NEM/%u: %04x:%08RX64 efl=%#08RX64 <=\n", pVCpu->idCpu, pCtx->cs.Sel, pCtx->rip, pCtx->rflags));
# ifdef LOG_ENABLED
    if (LogIs3Enabled())
        nemHCWinLogState(pVM, pVCpu);
# endif
# ifdef IN_RING0
    Assert(pVCpu->idCpu == pGVCpu->idCpu);
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
# ifdef NEM_WIN_USE_OUR_OWN_RUN_API
    VID_MESSAGE_MAPPING_HEADER volatile *pMappingHeader = (VID_MESSAGE_MAPPING_HEADER volatile *)pVCpu->nem.s.pvMsgSlotMapping;
    uint32_t        cMillies            = 5000; /** @todo lower this later... */
# endif
    const bool      fSingleStepping     = DBGFIsStepping(pVCpu);
//    const uint32_t  fCheckVmFFs         = !fSingleStepping ? VM_FF_HP_R0_PRE_HM_MASK
//                                                           : VM_FF_HP_R0_PRE_HM_STEP_MASK;
//    const uint32_t  fCheckCpuFFs        = !fSingleStepping ? VMCPU_FF_HP_R0_PRE_HM_MASK : VMCPU_FF_HP_R0_PRE_HM_STEP_MASK;
    VBOXSTRICTRC    rcStrict            = VINF_SUCCESS;
    for (unsigned iLoop = 0;; iLoop++)
    {
# ifndef NEM_WIN_USE_HYPERCALLS_FOR_PAGES
        /*
         * Hack alert!
         */
        uint32_t const cMappedPages = pVM->nem.s.cMappedPages;
        if (cMappedPages >= 4000)
        {
            PGMPhysNemEnumPagesByState(pVM, pVCpu, NEM_WIN_PAGE_STATE_READABLE, nemR3WinWHvUnmapOnePageCallback, NULL);
            Log(("nemHCWinRunGC: Unmapped all; cMappedPages=%u -> %u\n", cMappedPages, pVM->nem.s.cMappedPages));
        }
# endif

        /*
         * Pending interrupts or such?  Need to check and deal with this prior
         * to the state syncing.
         */
        pVCpu->nem.s.fDesiredInterruptWindows = 0;
        if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INTERRUPT_APIC | VMCPU_FF_UPDATE_APIC | VMCPU_FF_INTERRUPT_PIC
                                     | VMCPU_FF_INTERRUPT_NMI  | VMCPU_FF_INTERRUPT_SMI))
        {
# ifdef NEM_WIN_USE_OUR_OWN_RUN_API
            /* Make sure the CPU isn't executing. */
            if (pVCpu->nem.s.fHandleAndGetFlags == VID_MSHAGN_F_GET_NEXT_MESSAGE)
            {
                pVCpu->nem.s.fHandleAndGetFlags = 0;
                rcStrict = nemHCWinStopCpu(pVM, pVCpu, rcStrict, pMappingHeader, pGVM, pGVCpu);
                if (rcStrict == VINF_SUCCESS)
                { /* likely */ }
                else
                {
                    LogFlow(("NEM/%u: breaking: nemHCWinStopCpu -> %Rrc\n", pVCpu->idCpu, VBOXSTRICTRC_VAL(rcStrict) ));
                    STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatBreakOnStatus);
                    break;
                }
            }
# endif

            /* Try inject interrupt. */
            rcStrict = nemHCWinHandleInterruptFF(pVM, pVCpu, pGVCpu, pCtx, &pVCpu->nem.s.fDesiredInterruptWindows);
            if (rcStrict == VINF_SUCCESS)
            { /* likely */ }
            else
            {
                LogFlow(("NEM/%u: breaking: nemHCWinHandleInterruptFF -> %Rrc\n", pVCpu->idCpu, VBOXSTRICTRC_VAL(rcStrict) ));
                STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatBreakOnStatus);
                break;
            }
        }

        /*
         * Ensure that hyper-V has the whole state.
         * (We always update the interrupt windows settings when active as hyper-V seems
         * to forget about it after an exit.)
         */
        if (      (pCtx->fExtrn & (CPUMCTX_EXTRN_ALL | CPUMCTX_EXTRN_NEM_WIN_MASK))
               !=                 (CPUMCTX_EXTRN_ALL | CPUMCTX_EXTRN_NEM_WIN_MASK)
            || pVCpu->nem.s.fDesiredInterruptWindows
            || pVCpu->nem.s.fCurrentInterruptWindows != pVCpu->nem.s.fDesiredInterruptWindows)
        {
# ifdef NEM_WIN_USE_OUR_OWN_RUN_API
            Assert(pVCpu->nem.s.fHandleAndGetFlags != VID_MSHAGN_F_GET_NEXT_MESSAGE /* not running */);
# endif
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
# ifdef NEM_WIN_USE_OUR_OWN_RUN_API
            if (pVCpu->nem.s.fHandleAndGetFlags)
            { /* Very likely that the CPU does NOT need starting (pending msg, running). */ }
            else
            {
#  ifdef IN_RING0
                pVCpu->nem.s.uIoCtlBuf.idCpu = pGVCpu->idCpu;
                NTSTATUS rcNt = nemR0NtPerformIoControl(pGVM, pGVM->nem.s.IoCtlStartVirtualProcessor.uFunction,
                                                        &pVCpu->nem.s.uIoCtlBuf.idCpu, sizeof(pVCpu->nem.s.uIoCtlBuf.idCpu),
                                                        NULL, 0);
                LogFlow(("NEM/%u: IoCtlStartVirtualProcessor -> %#x\n", pVCpu->idCpu, rcNt));
                AssertLogRelMsgReturn(NT_SUCCESS(rcNt), ("VidStartVirtualProcessor failed for CPU #%u: %#x\n", pGVCpu->idCpu, rcNt),
                                      VERR_NEM_IPE_5);
#  else
                AssertLogRelMsgReturn(g_pfnVidStartVirtualProcessor(pVM->nem.s.hPartitionDevice, pVCpu->idCpu),
                                      ("VidStartVirtualProcessor failed for CPU #%u: %u (%#x, rcNt=%#x)\n",
                                       pVCpu->idCpu, RTNtLastErrorValue(), RTNtLastErrorValue(), RTNtLastStatusValue()),
                                      VERR_NEM_IPE_5);
#  endif
                pVCpu->nem.s.fHandleAndGetFlags = VID_MSHAGN_F_GET_NEXT_MESSAGE;
            }
# endif /* NEM_WIN_USE_OUR_OWN_RUN_API */

            if (VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC_NEM_WAIT, VMCPUSTATE_STARTED_EXEC_NEM))
            {
# ifdef NEM_WIN_USE_OUR_OWN_RUN_API
#  ifdef IN_RING0
                pVCpu->nem.s.uIoCtlBuf.MsgSlotHandleAndGetNext.iCpu     = pGVCpu->idCpu;
                pVCpu->nem.s.uIoCtlBuf.MsgSlotHandleAndGetNext.fFlags   = pVCpu->nem.s.fHandleAndGetFlags;
                pVCpu->nem.s.uIoCtlBuf.MsgSlotHandleAndGetNext.cMillies = cMillies;
                NTSTATUS rcNt = nemR0NtPerformIoControl(pGVM, pGVM->nem.s.IoCtlMessageSlotHandleAndGetNext.uFunction,
                                                        &pVCpu->nem.s.uIoCtlBuf.MsgSlotHandleAndGetNext,
                                                        sizeof(pVCpu->nem.s.uIoCtlBuf.MsgSlotHandleAndGetNext),
                                                        NULL, 0);
                VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC_NEM, VMCPUSTATE_STARTED_EXEC_NEM_WAIT);
                if (rcNt == STATUS_SUCCESS)
#  else
                BOOL fRet = VidMessageSlotHandleAndGetNext(pVM->nem.s.hPartitionDevice, pVCpu->idCpu,
                                                           pVCpu->nem.s.fHandleAndGetFlags, cMillies);
                VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC_NEM, VMCPUSTATE_STARTED_EXEC_NEM_WAIT);
                if (fRet)
#  endif
# else
                WHV_RUN_VP_EXIT_CONTEXT ExitReason;
                RT_ZERO(ExitReason);
                HRESULT hrc = WHvRunVirtualProcessor(pVM->nem.s.hPartition, pVCpu->idCpu, &ExitReason, sizeof(ExitReason));
                VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC_NEM, VMCPUSTATE_STARTED_EXEC_NEM_WAIT);
                if (SUCCEEDED(hrc))
# endif
                {
                    /*
                     * Deal with the message.
                     */
# ifdef NEM_WIN_USE_OUR_OWN_RUN_API
                    rcStrict = nemHCWinHandleMessage(pVM, pVCpu, pMappingHeader, pCtx, pGVCpu);
                    pVCpu->nem.s.fHandleAndGetFlags |= VID_MSHAGN_F_HANDLE_MESSAGE;
# else
                    rcStrict = nemR3WinHandleExit(pVM, pVCpu, &ExitReason, pCtx);
# endif
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
# ifdef NEM_WIN_USE_OUR_OWN_RUN_API

                    /* VID.SYS merges STATUS_ALERTED and STATUS_USER_APC into STATUS_TIMEOUT,
                       so after NtAlertThread we end up here with a STATUS_TIMEOUT.  And yeah,
                       the error code conversion is into WAIT_XXX, i.e. NT status codes. */
#  ifndef IN_RING0
                    DWORD rcNt = GetLastError();
#  endif
                    LogFlow(("NEM/%u: VidMessageSlotHandleAndGetNext -> %#x\n", pVCpu->idCpu, rcNt));
                    AssertLogRelMsgReturn(   rcNt == STATUS_TIMEOUT
                                          || rcNt == STATUS_ALERTED  /* just in case */
                                          || rcNt == STATUS_USER_APC /* ditto */
                                          , ("VidMessageSlotHandleAndGetNext failed for CPU #%u: %#x (%u)\n",
                                             pVCpu->idCpu, rcNt, rcNt),
                                          VERR_NEM_IPE_0);
                    pVCpu->nem.s.fHandleAndGetFlags = VID_MSHAGN_F_GET_NEXT_MESSAGE;
                    STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatGetMsgTimeout);
# else
                    AssertLogRelMsgFailedReturn(("WHvRunVirtualProcessor failed for CPU #%u: %#x (%u)\n",
                                                 pVCpu->idCpu, hrc, GetLastError()),
                                                VERR_NEM_IPE_0);

# endif
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
# ifdef NEM_WIN_USE_OUR_OWN_RUN_API
    if (pVCpu->nem.s.fHandleAndGetFlags == VID_MSHAGN_F_GET_NEXT_MESSAGE)
    {
        pVCpu->nem.s.fHandleAndGetFlags = 0;
        rcStrict = nemHCWinStopCpu(pVM, pVCpu, rcStrict, pMappingHeader, pGVM, pGVCpu);
    }
# endif

    if (!VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED, VMCPUSTATE_STARTED_EXEC_NEM))
        VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED, VMCPUSTATE_STARTED_EXEC_NEM_CANCELED);

    if (pCtx->fExtrn & (CPUMCTX_EXTRN_ALL | (CPUMCTX_EXTRN_NEM_WIN_MASK & ~CPUMCTX_EXTRN_NEM_WIN_EVENT_INJECT)))
    {
# ifdef IN_RING0
        int rc2 = nemR0WinImportState(pGVM, pGVCpu, pCtx, CPUMCTX_EXTRN_ALL | CPUMCTX_EXTRN_NEM_WIN_MASK);
        if (RT_SUCCESS(rc2))
            pCtx->fExtrn = 0;
        else if (rc2 == VERR_NEM_CHANGE_PGM_MODE || rc2 == VERR_NEM_FLUSH_TLB || rc2 == VERR_NEM_UPDATE_APIC_BASE)
        {
            pCtx->fExtrn = 0;
            if (rcStrict == VINF_SUCCESS || rcStrict == -rc2)
                rcStrict = -rc2;
            else
            {
                pVCpu->nem.s.rcPending = -rc2;
                LogFlow(("NEM/%u: rcPending=%Rrc (rcStrict=%Rrc)\n", pVCpu->idCpu, rc2, VBOXSTRICTRC_VAL(rcStrict) ));
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

#endif /* defined(NEM_WIN_USE_OUR_OWN_RUN_API) || defined(IN_RING3) */

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
            return VERR_NEM_IPE_2;
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
    return VERR_NEM_IPE_6;
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

