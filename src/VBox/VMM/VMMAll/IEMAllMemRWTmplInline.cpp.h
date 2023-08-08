/* $Id$ */
/** @file
 * IEM - Interpreted Execution Manager - Inlined R/W Memory Functions Template.
 */

/*
 * Copyright (C) 2011-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/* Check template parameters. */
#ifndef TMPL_MEM_TYPE
# error "TMPL_MEM_TYPE is undefined"
#endif
#ifndef TMPL_MEM_TYPE_ALIGN
# define TMPL_MEM_TYPE_ALIGN     (sizeof(TMPL_MEM_TYPE) - 1)
#endif
#ifndef TMPL_MEM_FN_SUFF
# error "TMPL_MEM_FN_SUFF is undefined"
#endif
#ifndef TMPL_MEM_FMT_TYPE
# error "TMPL_MEM_FMT_TYPE is undefined"
#endif
#ifndef TMPL_MEM_FMT_DESC
# error "TMPL_MEM_FMT_DESC is undefined"
#endif

/** @todo fix logging   */

#ifdef IEM_WITH_SETJMP

/**
 * Inlined fetch function that longjumps on error.
 *
 * @note The @a iSegRef is not allowed to be UINT8_MAX!
 */
DECLINLINE(TMPL_MEM_TYPE)
RT_CONCAT3(iemMemFetchData,TMPL_MEM_FN_SUFF,Jmp)(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP
{
# if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3) && !defined(TMPL_MEM_NO_INLINE)
    /*
     * Convert from segmented to flat address and check that it doesn't cross a page boundrary.
     */
    RTGCPTR GCPtrEff = iemMemApplySegmentToReadJmp(pVCpu, iSegReg, sizeof(TMPL_MEM_TYPE), GCPtrMem);
    if (RT_LIKELY((GCPtrEff & GUEST_PAGE_OFFSET_MASK) <= GUEST_PAGE_SIZE - sizeof(TMPL_MEM_TYPE)))
    {
        /*
         * TLB lookup.
         */
        uint64_t const uTag  = IEMTLB_CALC_TAG(    &pVCpu->iem.s.DataTlb, GCPtrEff);
        PIEMTLBENTRY   pTlbe = IEMTLB_TAG_TO_ENTRY(&pVCpu->iem.s.DataTlb, uTag);
        if (pTlbe->uTag == uTag)
        {
            /*
             * Check TLB page table level access flags.
             */
            AssertCompile(IEMTLBE_F_PT_NO_USER == 4);
            uint64_t const fNoUser = (IEM_GET_CPL(pVCpu) + 1) & IEMTLBE_F_PT_NO_USER;
            if (   (pTlbe->fFlagsAndPhysRev & (  IEMTLBE_F_PHYS_REV       | IEMTLBE_F_PG_UNASSIGNED | IEMTLBE_F_PG_NO_READ
                                               | IEMTLBE_F_PT_NO_ACCESSED | IEMTLBE_F_NO_MAPPINGR3  | fNoUser))
                == pVCpu->iem.s.DataTlb.uTlbPhysRev)
            {
                STAM_STATS({pVCpu->iem.s.DataTlb.cTlbHits++;});

                /*
                 * Alignment check:
                 */
                /** @todo check priority \#AC vs \#PF */
                AssertCompile(X86_CR0_AM == X86_EFL_AC);
                if (   !(GCPtrEff & TMPL_MEM_TYPE_ALIGN)
                    || !((uint32_t)pVCpu->cpum.GstCtx.cr0 & pVCpu->cpum.GstCtx.eflags.u & X86_CR0_AM)
                    || IEM_GET_CPL(pVCpu) != 3)
                {
                    /*
                     * Fetch and return the dword
                     */
                    Assert(pTlbe->pbMappingR3); /* (Only ever cleared by the owning EMT.) */
                    Assert(!((uintptr_t)pTlbe->pbMappingR3 & GUEST_PAGE_OFFSET_MASK));
                    TMPL_MEM_TYPE const uRet = *(TMPL_MEM_TYPE const *)&pTlbe->pbMappingR3[GCPtrEff & GUEST_PAGE_OFFSET_MASK];
                    Log9(("IEM RD " TMPL_MEM_FMT_DESC " %d|%RGv: " TMPL_MEM_FMT_TYPE "\n", iSegReg, GCPtrMem, uRet));
                    return uRet;
                }
                Log10Func(("Raising #AC for %RGv\n", GCPtrEff));
                iemRaiseAlignmentCheckExceptionJmp(pVCpu);
            }
        }
    }

    /* Fall back on the slow careful approach in case of TLB miss, MMIO, exception
       outdated page pointer, or other troubles.  (This will do a TLB load.) */
    Log10Func(("%u:%RGv falling back\n", iSegReg, GCPtrMem));
# endif
    return RT_CONCAT3(iemMemFetchData,TMPL_MEM_FN_SUFF,SafeJmp)(pVCpu, iSegReg, GCPtrMem);
}


/**
 * Inlined flat addressing fetch function that longjumps on error.
 */
DECLINLINE(TMPL_MEM_TYPE)
RT_CONCAT3(iemMemFlatFetchData,TMPL_MEM_FN_SUFF,Jmp)(PVMCPUCC pVCpu, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP
{
# if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3) && !defined(TMPL_MEM_NO_INLINE)
    /*
     * Check that it doesn't cross a page boundrary.
     */
    if (RT_LIKELY((GCPtrMem & GUEST_PAGE_OFFSET_MASK) <= GUEST_PAGE_SIZE - sizeof(TMPL_MEM_TYPE)))
    {
        /*
         * TLB lookup.
         */
        uint64_t const uTag  = IEMTLB_CALC_TAG(    &pVCpu->iem.s.DataTlb, GCPtrMem);
        PIEMTLBENTRY   pTlbe = IEMTLB_TAG_TO_ENTRY(&pVCpu->iem.s.DataTlb, uTag);
        if (pTlbe->uTag == uTag)
        {
            /*
             * Check TLB page table level access flags.
             */
            AssertCompile(IEMTLBE_F_PT_NO_USER == 4);
            uint64_t const fNoUser = (IEM_GET_CPL(pVCpu) + 1) & IEMTLBE_F_PT_NO_USER;
            if (   (pTlbe->fFlagsAndPhysRev & (  IEMTLBE_F_PHYS_REV       | IEMTLBE_F_PG_UNASSIGNED | IEMTLBE_F_PG_NO_READ
                                               | IEMTLBE_F_PT_NO_ACCESSED | IEMTLBE_F_NO_MAPPINGR3  | fNoUser))
                == pVCpu->iem.s.DataTlb.uTlbPhysRev)
            {
                STAM_STATS({pVCpu->iem.s.DataTlb.cTlbHits++;});

                /*
                 * Alignment check:
                 */
                /** @todo check priority \#AC vs \#PF */
                AssertCompile(X86_CR0_AM == X86_EFL_AC);
                if (   !(GCPtrMem & TMPL_MEM_TYPE_ALIGN)
                    || !((uint32_t)pVCpu->cpum.GstCtx.cr0 & pVCpu->cpum.GstCtx.eflags.u & X86_CR0_AM)
                    || IEM_GET_CPL(pVCpu) != 3)
                {
                    /*
                     * Fetch and return the dword
                     */
                    Assert(pTlbe->pbMappingR3); /* (Only ever cleared by the owning EMT.) */
                    Assert(!((uintptr_t)pTlbe->pbMappingR3 & GUEST_PAGE_OFFSET_MASK));
                    TMPL_MEM_TYPE const uRet = *(TMPL_MEM_TYPE const *)&pTlbe->pbMappingR3[GCPtrMem & GUEST_PAGE_OFFSET_MASK];
                    Log9(("IEM RD " TMPL_MEM_FMT_DESC " %RGv: " TMPL_MEM_FMT_TYPE "\n", GCPtrMem, uRet));
                    return uRet;
                }
                Log10Func(("Raising #AC for %RGv\n", GCPtrMem));
                iemRaiseAlignmentCheckExceptionJmp(pVCpu);
            }
        }
    }

    /* Fall back on the slow careful approach in case of TLB miss, MMIO, exception
       outdated page pointer, or other troubles.  (This will do a TLB load.) */
    Log10Func(("%RGv falling back\n", GCPtrMem));
# endif
    return RT_CONCAT3(iemMemFetchData,TMPL_MEM_FN_SUFF,SafeJmp)(pVCpu, UINT8_MAX, GCPtrMem);
}

# ifndef TMPL_MEM_NO_STORE

/**
 * Inlined store function that longjumps on error.
 *
 * @note The @a iSegRef is not allowed to be UINT8_MAX!
 */
DECLINLINE(void)
RT_CONCAT3(iemMemStoreData,TMPL_MEM_FN_SUFF,Jmp)(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem,
                                                 TMPL_MEM_TYPE uValue) IEM_NOEXCEPT_MAY_LONGJMP
{
#  if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3) && !defined(TMPL_MEM_NO_INLINE)
    /*
     * Convert from segmented to flat address and check that it doesn't cross a page boundrary.
     */
    RTGCPTR GCPtrEff = iemMemApplySegmentToWriteJmp(pVCpu, iSegReg, sizeof(TMPL_MEM_TYPE), GCPtrMem);
    if (RT_LIKELY((GCPtrEff & GUEST_PAGE_OFFSET_MASK) <= GUEST_PAGE_SIZE - sizeof(TMPL_MEM_TYPE)))
    {
        /*
         * TLB lookup.
         */
        uint64_t const uTag  = IEMTLB_CALC_TAG(    &pVCpu->iem.s.DataTlb, GCPtrEff);
        PIEMTLBENTRY   pTlbe = IEMTLB_TAG_TO_ENTRY(&pVCpu->iem.s.DataTlb, uTag);
        if (pTlbe->uTag == uTag)
        {
            /*
             * Check TLB page table level access flags.
             */
            AssertCompile(IEMTLBE_F_PT_NO_USER == 4);
            uint64_t const fNoUser = (IEM_GET_CPL(pVCpu) + 1) & IEMTLBE_F_PT_NO_USER;
            if (   (pTlbe->fFlagsAndPhysRev & (  IEMTLBE_F_PHYS_REV       | IEMTLBE_F_PG_UNASSIGNED | IEMTLBE_F_PG_NO_WRITE
                                               | IEMTLBE_F_PT_NO_ACCESSED | IEMTLBE_F_PT_NO_DIRTY   | IEMTLBE_F_PT_NO_WRITE
                                               | IEMTLBE_F_NO_MAPPINGR3   | fNoUser))
                == pVCpu->iem.s.DataTlb.uTlbPhysRev)
            {
                STAM_STATS({pVCpu->iem.s.DataTlb.cTlbHits++;});

                /*
                 * Alignment check:
                 */
                /** @todo check priority \#AC vs \#PF */
                AssertCompile(X86_CR0_AM == X86_EFL_AC);
                if (   !(GCPtrEff & TMPL_MEM_TYPE_ALIGN)
                    || !((uint32_t)pVCpu->cpum.GstCtx.cr0 & pVCpu->cpum.GstCtx.eflags.u & X86_CR0_AM)
                    || IEM_GET_CPL(pVCpu) != 3)
                {
                    /*
                     * Store the dword and return.
                     */
                    Assert(pTlbe->pbMappingR3); /* (Only ever cleared by the owning EMT.) */
                    Assert(!((uintptr_t)pTlbe->pbMappingR3 & GUEST_PAGE_OFFSET_MASK));
                    *(TMPL_MEM_TYPE *)&pTlbe->pbMappingR3[GCPtrEff & GUEST_PAGE_OFFSET_MASK] = uValue;
                    Log9(("IEM WR " TMPL_MEM_FMT_DESC " %d|%RGv: " TMPL_MEM_FMT_TYPE "\n", iSegReg, GCPtrMem, uValue));
                    return;
                }
                Log10Func(("Raising #AC for %RGv\n", GCPtrEff));
                iemRaiseAlignmentCheckExceptionJmp(pVCpu);
            }
        }
    }

    /* Fall back on the slow careful approach in case of TLB miss, MMIO, exception
       outdated page pointer, or other troubles.  (This will do a TLB load.) */
    Log10Func(("%u:%RGv falling back\n", iSegReg, GCPtrMem));
#  endif
    RT_CONCAT3(iemMemStoreData,TMPL_MEM_FN_SUFF,SafeJmp)(pVCpu, iSegReg, GCPtrMem, uValue);
}


/**
 * Inlined flat addressing store function that longjumps on error.
 */
DECLINLINE(void)
RT_CONCAT3(iemMemFlatStoreData,TMPL_MEM_FN_SUFF,Jmp)(PVMCPUCC pVCpu, RTGCPTR GCPtrMem,
                                                     TMPL_MEM_TYPE uValue) IEM_NOEXCEPT_MAY_LONGJMP
{
#  if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3) && !defined(TMPL_MEM_NO_INLINE)
    /*
     * Check that it doesn't cross a page boundrary.
     */
    if (RT_LIKELY((GCPtrMem & GUEST_PAGE_OFFSET_MASK) <= GUEST_PAGE_SIZE - sizeof(TMPL_MEM_TYPE)))
    {
        /*
         * TLB lookup.
         */
        uint64_t const uTag  = IEMTLB_CALC_TAG(    &pVCpu->iem.s.DataTlb, GCPtrMem);
        PIEMTLBENTRY   pTlbe = IEMTLB_TAG_TO_ENTRY(&pVCpu->iem.s.DataTlb, uTag);
        if (pTlbe->uTag == uTag)
        {
            /*
             * Check TLB page table level access flags.
             */
            AssertCompile(IEMTLBE_F_PT_NO_USER == 4);
            uint64_t const fNoUser = (IEM_GET_CPL(pVCpu) + 1) & IEMTLBE_F_PT_NO_USER;
            if (   (pTlbe->fFlagsAndPhysRev & (  IEMTLBE_F_PHYS_REV       | IEMTLBE_F_PG_UNASSIGNED | IEMTLBE_F_PG_NO_WRITE
                                               | IEMTLBE_F_PT_NO_ACCESSED | IEMTLBE_F_PT_NO_DIRTY   | IEMTLBE_F_PT_NO_WRITE
                                               | IEMTLBE_F_NO_MAPPINGR3   | fNoUser))
                == pVCpu->iem.s.DataTlb.uTlbPhysRev)
            {
                STAM_STATS({pVCpu->iem.s.DataTlb.cTlbHits++;});

                /*
                 * Alignment check:
                 */
                /** @todo check priority \#AC vs \#PF */
                AssertCompile(X86_CR0_AM == X86_EFL_AC);
                if (   !(GCPtrMem & TMPL_MEM_TYPE_ALIGN)
                    || !((uint32_t)pVCpu->cpum.GstCtx.cr0 & pVCpu->cpum.GstCtx.eflags.u & X86_CR0_AM)
                    || IEM_GET_CPL(pVCpu) != 3)
                {
                    /*
                     * Store the dword and return.
                     */
                    Assert(pTlbe->pbMappingR3); /* (Only ever cleared by the owning EMT.) */
                    Assert(!((uintptr_t)pTlbe->pbMappingR3 & GUEST_PAGE_OFFSET_MASK));
                    *(TMPL_MEM_TYPE *)&pTlbe->pbMappingR3[GCPtrMem & GUEST_PAGE_OFFSET_MASK] = uValue;
                    Log9(("IEM WR " TMPL_MEM_FMT_DESC " %RGv: " TMPL_MEM_FMT_TYPE "\n", GCPtrMem, uValue));
                    return;
                }
                Log10Func(("Raising #AC for %RGv\n", GCPtrMem));
                iemRaiseAlignmentCheckExceptionJmp(pVCpu);
            }
        }
    }

    /* Fall back on the slow careful approach in case of TLB miss, MMIO, exception
       outdated page pointer, or other troubles.  (This will do a TLB load.) */
    Log10Func(("%RGv falling back\n", GCPtrMem));
#  endif
    RT_CONCAT3(iemMemStoreData,TMPL_MEM_FN_SUFF,SafeJmp)(pVCpu, UINT8_MAX, GCPtrMem, uValue);
}

# endif /* !TMPL_MEM_NO_STORE */

#endif /* IEM_WITH_SETJMP */

#undef TMPL_MEM_TYPE
#undef TMPL_MEM_TYPE_ALIGN
#undef TMPL_MEM_FN_SUFF
#undef TMPL_MEM_FMT_TYPE
#undef TMPL_MEM_FMT_DESC

