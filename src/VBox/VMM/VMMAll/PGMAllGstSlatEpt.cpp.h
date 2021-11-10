/* $Id$ */
/** @file
 * VBox - Page Manager, Guest EPT SLAT - All context code.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#if PGM_GST_TYPE == PGM_TYPE_EPT
DECLINLINE(int) PGM_GST_SLAT_NAME_EPT(WalkReturnNotPresent)(PVMCPUCC pVCpu, PGSTPTWALK pWalk, int iLevel)
{
    NOREF(pVCpu);
    pWalk->Core.fNotPresent     = true;
    pWalk->Core.uLevel          = (uint8_t)iLevel;
    return VERR_PAGE_TABLE_NOT_PRESENT;
}


DECLINLINE(int) PGM_GST_SLAT_NAME_EPT(WalkReturnBadPhysAddr)(PVMCPUCC pVCpu, PGSTPTWALK pWalk, int iLevel, int rc)
{
    AssertMsg(rc == VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS, ("%Rrc\n", rc)); NOREF(rc); NOREF(pVCpu);
    pWalk->Core.fBadPhysAddr    = true;
    pWalk->Core.uLevel          = (uint8_t)iLevel;
    return VERR_PAGE_TABLE_NOT_PRESENT;
}


DECLINLINE(int) PGM_GST_SLAT_NAME_EPT(WalkReturnRsvdError)(PVMCPUCC pVCpu, PGSTPTWALK pWalk, int iLevel)
{
    NOREF(pVCpu);
    pWalk->Core.fRsvdError      = true;
    pWalk->Core.uLevel          = (uint8_t)iLevel;
    return VERR_PAGE_TABLE_NOT_PRESENT;
}


DECLINLINE(int) PGM_GST_SLAT_NAME_EPT(Walk)(PVMCPUCC pVCpu, RTGCPHYS GCPhysNested, bool fIsLinearAddrValid, RTGCPTR GCPtrNested,
                                            PGSTPTWALK pWalk)
{
    /*
     * Init walk structure.
     */
    int rc;
    RT_ZERO(*pWalk);
    pWalk->Core.GCPtr              = GCPtrNested;
    pWalk->Core.GCPhysNested       = GCPhysNested;
    pWalk->Core.fIsSlat            = true;
    pWalk->Core.fIsLinearAddrValid = fIsLinearAddrValid;

    /*
     * Figure out EPT attributes that are cumulative (logical-AND) across page walks.
     *   - R, W, X_SUPER are unconditionally cumulative.
     *     See Intel spec. Table 26-7 "Exit Qualification for EPT Violations".
     *
     *   - X_USER is Cumulative but relevant only when mode-based execute control for EPT
     *     which we currently don't support it (asserted below).
     *
     *   - MEMTYPE is not cumulative and only applicable to the final paging entry.
     *
     *   - A, D EPT bits map to the regular page-table bit positions. Thus, they're not
     *     included in the mask below and handled separately. Accessed bits are
     *     cumulative but dirty bits are not cumulative as they're only applicable to
     *     the final paging entry.
     */
    Assert(!pVCpu->CTX_SUFF(pVM)->cpum.ro.GuestFeatures.fVmxModeBasedExecuteEpt);
    uint64_t const fCumulativeEpt = PGM_PTATTRS_EPT_R_MASK
                                  | PGM_PTATTRS_EPT_W_MASK
                                  | PGM_PTATTRS_EPT_X_SUPER_MASK;

    /*
     * Do the walk.
     */
    uint64_t fEffective;
    {
        rc = pgmGstGetEptPML4PtrEx(pVCpu, &pWalk->pPml4);
        if (RT_SUCCESS(rc)) { /* probable */ }
        else return PGM_GST_SLAT_NAME_EPT(WalkReturnBadPhysAddr)(pVCpu, pWalk, 4, rc);

        PEPTPML4E pPml4e;
        pWalk->pPml4e = pPml4e = &pWalk->pPml4->a[(GCPhysNested >> EPT_PML4_SHIFT) & EPT_PML4_MASK];
        EPTPML4E  Pml4e;
        pWalk->Pml4e.u = Pml4e.u = pPml4e->u;

        if (GST_IS_PGENTRY_PRESENT(pVCpu, Pml4e)) { /* probable */ }
        else return PGM_GST_SLAT_NAME_EPT(WalkReturnNotPresent)(pVCpu, pWalk, 4);

        if (RT_LIKELY(GST_IS_PML4E_VALID(pVCpu, Pml4e))) { /* likely */ }
        else return PGM_GST_SLAT_NAME_EPT(WalkReturnRsvdError)(pVCpu, pWalk, 4);

        Assert(!pVCpu->CTX_SUFF(pVM)->cpum.ro.GuestFeatures.fVmxModeBasedExecuteEpt);
        uint64_t const fEptAttrs     = Pml4e.u & EPT_PML4E_ATTR_MASK;
        uint8_t const fAccessed      = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_ACCESSED);
        uint64_t const fEffectiveEpt = (fEptAttrs << PGM_PTATTRS_EPT_SHIFT) & PGM_PTATTRS_EPT_MASK;
        fEffective = RT_BF_MAKE(PGM_PTATTRS_A, fAccessed)
                   | fEffectiveEpt;
        pWalk->Core.fEffective = fEffective;

        rc = PGM_GCPHYS_2_PTR_BY_VMCPU(pVCpu, Pml4e.u & EPT_PML4E_PG_MASK, &pWalk->pPdpt);
        if (RT_SUCCESS(rc)) { /* probable */ }
        else return PGM_GST_SLAT_NAME_EPT(WalkReturnBadPhysAddr)(pVCpu, pWalk, 3, rc);
    }
    {
        PEPTPDPTE pPdpte;
        pWalk->pPdpte = pPdpte = &pWalk->pPdpt->a[(GCPhysNested >> GST_PDPT_SHIFT) & GST_PDPT_MASK];
        EPTPDPTE  Pdpte;
        pWalk->Pdpte.u = Pdpte.u = pPdpte->u;

        if (GST_IS_PGENTRY_PRESENT(pVCpu, Pdpte)) { /* probable */ }
        else return PGM_GST_SLAT_NAME_EPT(WalkReturnNotPresent)(pVCpu, pWalk, 3);

        /* The order of the following 2 "if" statements matter. */
        if (GST_IS_PDPE_VALID(pVCpu, Pdpte))
        {
            uint64_t const fEptAttrs     = Pdpte.u & EPT_PDPTE_ATTR_MASK;
            uint8_t const  fAccessed     = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_ACCESSED);
            uint64_t const fEffectiveEpt = (fEptAttrs << PGM_PTATTRS_EPT_SHIFT) & PGM_PTATTRS_EPT_MASK;
            fEffective &= RT_BF_MAKE(PGM_PTATTRS_A, fAccessed)
                       |  (fEffectiveEpt & fCumulativeEpt);
            pWalk->Core.fEffective = fEffective;
        }
        else if (GST_IS_BIG_PDPE_VALID(pVCpu, Pdpte))
        {
            uint64_t const fEptAttrs     = Pdpte.u & EPT_PDPTE1G_ATTR_MASK;
            uint8_t const fAccessed      = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_ACCESSED);
            uint8_t const fDirty         = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_DIRTY);
            uint8_t const fMemType       = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_MEMTYPE);
            uint64_t const fEffectiveEpt = (fEptAttrs << PGM_PTATTRS_EPT_SHIFT) & PGM_PTATTRS_EPT_MASK;
            fEffective &= RT_BF_MAKE(PGM_PTATTRS_A,           fAccessed)
                       |  (fEffectiveEpt & fCumulativeEpt);
            fEffective |= RT_BF_MAKE(PGM_PTATTRS_D,           fDirty)
                       |  RT_BF_MAKE(PGM_PTATTRS_EPT_MEMTYPE, fMemType);
            pWalk->Core.fEffective = fEffective;

            pWalk->Core.fEffectiveRW = !!(fEffective & PGM_PTATTRS_RW_MASK);    /** @todo RW isn't copied from EPT R, W. This will break callers who use RW for EPT attributes. */
            pWalk->Core.fEffectiveUS = true;
            pWalk->Core.fGigantPage  = true;
            pWalk->Core.fSucceeded   = true;
            pWalk->Core.GCPhys       = GST_GET_BIG_PDPE_GCPHYS(pVCpu->CTX_SUFF(pVM), Pdpte)
                                     | (GCPhysNested & GST_GIGANT_PAGE_OFFSET_MASK);
            PGM_A20_APPLY_TO_VAR(pVCpu, pWalk->Core.GCPhys);
            return VINF_SUCCESS;
        }
        else return PGM_GST_SLAT_NAME_EPT(WalkReturnRsvdError)(pVCpu, pWalk, 3);
    }
    {
        PGSTPDE pPde;
        pWalk->pPde  = pPde  = &pWalk->pPd->a[(GCPhysNested >> GST_PD_SHIFT) & GST_PD_MASK];
        GSTPDE  Pde;
        pWalk->Pde.u = Pde.u = pPde->u;
        if (GST_IS_PGENTRY_PRESENT(pVCpu, Pde)) { /* probable */ }
        else return PGM_GST_SLAT_NAME_EPT(WalkReturnNotPresent)(pVCpu, pWalk, 2);
        if ((Pde.u & X86_PDE_PS) && GST_IS_PSE_ACTIVE(pVCpu))
        {
            if (RT_LIKELY(GST_IS_BIG_PDE_VALID(pVCpu, Pde))) { /* likely */ }
            else return PGM_GST_SLAT_NAME_EPT(WalkReturnRsvdError)(pVCpu, pWalk, 2);

            uint64_t const fEptAttrs     = Pde.u & EPT_PDE2M_ATTR_MASK;
            uint8_t const fAccessed      = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_ACCESSED);
            uint8_t const fDirty         = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_DIRTY);
            uint8_t const fMemType       = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_MEMTYPE);
            uint64_t const fEffectiveEpt = (fEptAttrs << PGM_PTATTRS_EPT_SHIFT) & PGM_PTATTRS_EPT_MASK;

            fEffective &= RT_BF_MAKE(PGM_PTATTRS_A,           fAccessed)
                       |  (fEffectiveEpt & fCumulativeEpt);
            fEffective |= RT_BF_MAKE(PGM_PTATTRS_D,           fDirty)
                       |  RT_BF_MAKE(PGM_PTATTRS_EPT_MEMTYPE, fMemType);
            pWalk->Core.fEffective = fEffective;
            pWalk->Core.fEffectiveRW = !!(fEffective & PGM_PTATTRS_RW_MASK); /** @todo RW isn't copied from EPT R, W. This will break callers who use RW for EPT attributes. */
            pWalk->Core.fEffectiveUS = true;
            pWalk->Core.fBigPage     = true;
            pWalk->Core.fSucceeded   = true;
            pWalk->Core.GCPhys       = GST_GET_BIG_PDE_GCPHYS(pVCpu->CTX_SUFF(pVM), Pde)
                                     | (GCPhysNested & GST_BIG_PAGE_OFFSET_MASK);
            PGM_A20_APPLY_TO_VAR(pVCpu, pWalk->Core.GCPhys);
            return VINF_SUCCESS;
        }

        if (RT_UNLIKELY(!GST_IS_PDE_VALID(pVCpu, Pde)))
            return PGM_GST_SLAT_NAME_EPT(WalkReturnRsvdError)(pVCpu, pWalk, 2);

        uint64_t const fEptAttrs     = Pde.u & EPT_PDE_ATTR_MASK;
        uint8_t const fAccessed      = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_ACCESSED);
        uint64_t const fEffectiveEpt = (fEptAttrs << PGM_PTATTRS_EPT_SHIFT) & PGM_PTATTRS_EPT_MASK;

        fEffective &= RT_BF_MAKE(PGM_PTATTRS_A,  fAccessed)
                   |  (fEffectiveEpt & fCumulativeEpt);
        pWalk->Core.fEffective = fEffective;

        rc = PGM_GCPHYS_2_PTR_BY_VMCPU(pVCpu, GST_GET_PDE_GCPHYS(Pde), &pWalk->pPt);
        if (RT_SUCCESS(rc)) { /* probable */ }
        else return PGM_GST_SLAT_NAME_EPT(WalkReturnBadPhysAddr)(pVCpu, pWalk, 1, rc);
    }
    {
        PGSTPTE pPte;
        pWalk->pPte  = pPte  = &pWalk->pPt->a[(GCPhysNested >> GST_PT_SHIFT) & GST_PT_MASK];
        GSTPTE  Pte;
        pWalk->Pte.u = Pte.u = pPte->u;

        if (GST_IS_PGENTRY_PRESENT(pVCpu, Pte)) { /* probable */ }
        else return PGM_GST_SLAT_NAME_EPT(WalkReturnNotPresent)(pVCpu, pWalk, 1);

        if (RT_LIKELY(GST_IS_PTE_VALID(pVCpu, Pte))) { /* likely */ }
        else return PGM_GST_SLAT_NAME_EPT(WalkReturnRsvdError)(pVCpu, pWalk, 1);

        uint64_t const fEptAttrs     = Pte.u & EPT_PTE_ATTR_MASK;
        uint8_t const fAccessed      = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_ACCESSED);
        uint8_t const fDirty         = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_DIRTY);
        uint8_t const fMemType       = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_MEMTYPE);
        uint64_t const fEffectiveEpt = (fEptAttrs << PGM_PTATTRS_EPT_SHIFT) & PGM_PTATTRS_EPT_MASK;
        fEffective &= RT_BF_MAKE(PGM_PTATTRS_A,           fAccessed)
                   |  (fEffectiveEpt & fCumulativeEpt);
        fEffective |= RT_BF_MAKE(PGM_PTATTRS_D,           fDirty)
                   |  RT_BF_MAKE(PGM_PTATTRS_EPT_MEMTYPE, fMemType);
        pWalk->Core.fEffective = fEffective;

        pWalk->Core.fEffectiveRW = !!(fEffective & PGM_PTATTRS_RW_MASK); /** @todo RW isn't copied from EPT R, W. This will break callers who use RW for EPT attributes. */
        pWalk->Core.fEffectiveUS = true;
        pWalk->Core.fSucceeded   = true;
        pWalk->Core.GCPhys       = GST_GET_PTE_GCPHYS(Pte)
                                 | (GCPhysNested & PAGE_OFFSET_MASK);
        return VINF_SUCCESS;
    }
}
#else
# error "Guest paging type must be EPT."
#endif

