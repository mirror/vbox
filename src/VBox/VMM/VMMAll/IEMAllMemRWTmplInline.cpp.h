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
#ifndef TMPL_MEM_TYPE_SIZE
# error "TMPL_MEM_TYPE_SIZE is undefined"
#endif
#ifndef TMPL_MEM_TYPE_ALIGN
# error "TMPL_MEM_TYPE_ALIGN is undefined"
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

#if TMPL_MEM_TYPE_ALIGN + 1 < TMPL_MEM_TYPE_SIZE
# error Have not implemented TMPL_MEM_TYPE_ALIGN smaller than TMPL_MEM_TYPE_SIZE - 1.
#endif

/** @todo fix logging   */

#ifdef IEM_WITH_SETJMP


/*********************************************************************************************************************************
*   Fetches                                                                                                                      *
*********************************************************************************************************************************/

/**
 * Inlined fetch function that longjumps on error.
 *
 * @note The @a iSegRef is not allowed to be UINT8_MAX!
 */
DECL_INLINE_THROW(TMPL_MEM_TYPE)
RT_CONCAT3(iemMemFetchData,TMPL_MEM_FN_SUFF,Jmp)(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP
{
    AssertCompile(sizeof(TMPL_MEM_TYPE) == TMPL_MEM_TYPE_SIZE);
# if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3) && !defined(TMPL_MEM_NO_INLINE)
    /*
     * Convert from segmented to flat address and check that it doesn't cross a page boundrary.
     */
    RTGCPTR GCPtrEff = iemMemApplySegmentToReadJmp(pVCpu, iSegReg, sizeof(TMPL_MEM_TYPE), GCPtrMem);
#  if TMPL_MEM_TYPE_SIZE > 1
    if (RT_LIKELY(   !(GCPtrEff & TMPL_MEM_TYPE_ALIGN) /* If aligned, it will be within the page. */
                  || TMPL_MEM_CHECK_UNALIGNED_WITHIN_PAGE_OK(pVCpu, GCPtrEff, TMPL_MEM_TYPE) ))
#  endif
    {
        /*
         * TLB lookup.
         */
        uint64_t const uTag  = IEMTLB_CALC_TAG(    &pVCpu->iem.s.DataTlb, GCPtrEff);
        PIEMTLBENTRY   pTlbe = IEMTLB_TAG_TO_ENTRY(&pVCpu->iem.s.DataTlb, uTag);
        if (RT_LIKELY(pTlbe->uTag == uTag))
        {
            /*
             * Check TLB page table level access flags.
             */
            AssertCompile(IEMTLBE_F_PT_NO_USER == 4);
            uint64_t const fNoUser = (IEM_GET_CPL(pVCpu) + 1) & IEMTLBE_F_PT_NO_USER;
            if (RT_LIKELY(   (pTlbe->fFlagsAndPhysRev & (  IEMTLBE_F_PHYS_REV       | IEMTLBE_F_PG_UNASSIGNED | IEMTLBE_F_PG_NO_READ
                                                         | IEMTLBE_F_PT_NO_ACCESSED | IEMTLBE_F_NO_MAPPINGR3  | fNoUser))
                          == pVCpu->iem.s.DataTlb.uTlbPhysRev))
            {
                /*
                 * Fetch and return the data.
                 */
                STAM_STATS({pVCpu->iem.s.DataTlb.cTlbHits++;});
                Assert(pTlbe->pbMappingR3); /* (Only ever cleared by the owning EMT.) */
                Assert(!((uintptr_t)pTlbe->pbMappingR3 & GUEST_PAGE_OFFSET_MASK));
                TMPL_MEM_TYPE const uRet = *(TMPL_MEM_TYPE const *)&pTlbe->pbMappingR3[GCPtrEff & GUEST_PAGE_OFFSET_MASK];
                Log9(("IEM RD " TMPL_MEM_FMT_DESC " %d|%RGv: " TMPL_MEM_FMT_TYPE "\n", iSegReg, GCPtrMem, uRet));
                return uRet;
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
DECL_INLINE_THROW(TMPL_MEM_TYPE)
RT_CONCAT3(iemMemFlatFetchData,TMPL_MEM_FN_SUFF,Jmp)(PVMCPUCC pVCpu, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP
{
# if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3) && !defined(TMPL_MEM_NO_INLINE)
    /*
     * Check that it doesn't cross a page boundrary.
     */
#  if TMPL_MEM_TYPE_SIZE > 1
    AssertCompile(X86_CR0_AM == X86_EFL_AC);
    AssertCompile(((3U + 1U) << 16) == X86_CR0_AM);
    if (RT_LIKELY(   !(GCPtrMem & TMPL_MEM_TYPE_ALIGN) /* If aligned, it will be within the page. */
                  || TMPL_MEM_CHECK_UNALIGNED_WITHIN_PAGE_OK(pVCpu, GCPtrMem, TMPL_MEM_TYPE) ))
#  endif
    {
        /*
         * TLB lookup.
         */
        uint64_t const uTag  = IEMTLB_CALC_TAG(    &pVCpu->iem.s.DataTlb, GCPtrMem);
        PIEMTLBENTRY   pTlbe = IEMTLB_TAG_TO_ENTRY(&pVCpu->iem.s.DataTlb, uTag);
        if (RT_LIKELY(pTlbe->uTag == uTag))
        {
            /*
             * Check TLB page table level access flags.
             */
            AssertCompile(IEMTLBE_F_PT_NO_USER == 4);
            uint64_t const fNoUser = (IEM_GET_CPL(pVCpu) + 1) & IEMTLBE_F_PT_NO_USER;
            if (RT_LIKELY(   (pTlbe->fFlagsAndPhysRev & (  IEMTLBE_F_PHYS_REV       | IEMTLBE_F_PG_UNASSIGNED | IEMTLBE_F_PG_NO_READ
                                                         | IEMTLBE_F_PT_NO_ACCESSED | IEMTLBE_F_NO_MAPPINGR3  | fNoUser))
                          == pVCpu->iem.s.DataTlb.uTlbPhysRev))
            {
                /*
                 * Fetch and return the dword
                 */
                STAM_STATS({pVCpu->iem.s.DataTlb.cTlbHits++;});
                Assert(pTlbe->pbMappingR3); /* (Only ever cleared by the owning EMT.) */
                Assert(!((uintptr_t)pTlbe->pbMappingR3 & GUEST_PAGE_OFFSET_MASK));
                TMPL_MEM_TYPE const uRet = *(TMPL_MEM_TYPE const *)&pTlbe->pbMappingR3[GCPtrMem & GUEST_PAGE_OFFSET_MASK];
                Log9(("IEM RD " TMPL_MEM_FMT_DESC " %RGv: " TMPL_MEM_FMT_TYPE "\n", GCPtrMem, uRet));
                return uRet;
            }
        }
    }

    /* Fall back on the slow careful approach in case of TLB miss, MMIO, exception
       outdated page pointer, or other troubles.  (This will do a TLB load.) */
    Log10Func(("%RGv falling back\n", GCPtrMem));
# endif
    return RT_CONCAT3(iemMemFetchData,TMPL_MEM_FN_SUFF,SafeJmp)(pVCpu, UINT8_MAX, GCPtrMem);
}


/*********************************************************************************************************************************
*   Stores                                                                                                                       *
*********************************************************************************************************************************/
# ifndef TMPL_MEM_NO_STORE

/**
 * Inlined store function that longjumps on error.
 *
 * @note The @a iSegRef is not allowed to be UINT8_MAX!
 */
DECL_INLINE_THROW(void)
RT_CONCAT3(iemMemStoreData,TMPL_MEM_FN_SUFF,Jmp)(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem,
                                                 TMPL_MEM_TYPE uValue) IEM_NOEXCEPT_MAY_LONGJMP
{
#  if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3) && !defined(TMPL_MEM_NO_INLINE)
    /*
     * Convert from segmented to flat address and check that it doesn't cross a page boundrary.
     */
    RTGCPTR GCPtrEff = iemMemApplySegmentToWriteJmp(pVCpu, iSegReg, sizeof(TMPL_MEM_TYPE), GCPtrMem);
#  if TMPL_MEM_TYPE_SIZE > 1
    AssertCompile(X86_CR0_AM == X86_EFL_AC);
    AssertCompile(((3U + 1U) << 16) == X86_CR0_AM);
    if (RT_LIKELY(   !(GCPtrEff & TMPL_MEM_TYPE_ALIGN) /* If aligned, it will be within the page. */
                  || TMPL_MEM_CHECK_UNALIGNED_WITHIN_PAGE_OK(pVCpu, GCPtrEff, TMPL_MEM_TYPE) ))
#  endif
    {
        /*
         * TLB lookup.
         */
        uint64_t const uTag  = IEMTLB_CALC_TAG(    &pVCpu->iem.s.DataTlb, GCPtrEff);
        PIEMTLBENTRY   pTlbe = IEMTLB_TAG_TO_ENTRY(&pVCpu->iem.s.DataTlb, uTag);
        if (RT_LIKELY(pTlbe->uTag == uTag))
        {
            /*
             * Check TLB page table level access flags.
             */
            AssertCompile(IEMTLBE_F_PT_NO_USER == 4);
            uint64_t const fNoUser = (IEM_GET_CPL(pVCpu) + 1) & IEMTLBE_F_PT_NO_USER;
            if (RT_LIKELY(   (pTlbe->fFlagsAndPhysRev & (  IEMTLBE_F_PHYS_REV       | IEMTLBE_F_PG_UNASSIGNED | IEMTLBE_F_PG_NO_WRITE
                                                         | IEMTLBE_F_PT_NO_ACCESSED | IEMTLBE_F_PT_NO_DIRTY   | IEMTLBE_F_PT_NO_WRITE
                                                         | IEMTLBE_F_NO_MAPPINGR3   | fNoUser))
                          == pVCpu->iem.s.DataTlb.uTlbPhysRev))
            {
                /*
                 * Store the dword and return.
                 */
                STAM_STATS({pVCpu->iem.s.DataTlb.cTlbHits++;});
                Assert(pTlbe->pbMappingR3); /* (Only ever cleared by the owning EMT.) */
                Assert(!((uintptr_t)pTlbe->pbMappingR3 & GUEST_PAGE_OFFSET_MASK));
                *(TMPL_MEM_TYPE *)&pTlbe->pbMappingR3[GCPtrEff & GUEST_PAGE_OFFSET_MASK] = uValue;
                Log9(("IEM WR " TMPL_MEM_FMT_DESC " %d|%RGv: " TMPL_MEM_FMT_TYPE "\n", iSegReg, GCPtrMem, uValue));
                return;
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
DECL_INLINE_THROW(void)
RT_CONCAT3(iemMemFlatStoreData,TMPL_MEM_FN_SUFF,Jmp)(PVMCPUCC pVCpu, RTGCPTR GCPtrMem,
                                                     TMPL_MEM_TYPE uValue) IEM_NOEXCEPT_MAY_LONGJMP
{
#  if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3) && !defined(TMPL_MEM_NO_INLINE)
    /*
     * Check that it doesn't cross a page boundrary.
     */
#  if TMPL_MEM_TYPE_SIZE > 1
    if (RT_LIKELY(   !(GCPtrMem & TMPL_MEM_TYPE_ALIGN)
                  || TMPL_MEM_CHECK_UNALIGNED_WITHIN_PAGE_OK(pVCpu, GCPtrMem, TMPL_MEM_TYPE) ))
#  endif
    {
        /*
         * TLB lookup.
         */
        uint64_t const uTag  = IEMTLB_CALC_TAG(    &pVCpu->iem.s.DataTlb, GCPtrMem);
        PIEMTLBENTRY   pTlbe = IEMTLB_TAG_TO_ENTRY(&pVCpu->iem.s.DataTlb, uTag);
        if (RT_LIKELY(pTlbe->uTag == uTag))
        {
            /*
             * Check TLB page table level access flags.
             */
            AssertCompile(IEMTLBE_F_PT_NO_USER == 4);
            uint64_t const fNoUser = (IEM_GET_CPL(pVCpu) + 1) & IEMTLBE_F_PT_NO_USER;
            if (RT_LIKELY(   (pTlbe->fFlagsAndPhysRev & (  IEMTLBE_F_PHYS_REV       | IEMTLBE_F_PG_UNASSIGNED | IEMTLBE_F_PG_NO_WRITE
                                                         | IEMTLBE_F_PT_NO_ACCESSED | IEMTLBE_F_PT_NO_DIRTY   | IEMTLBE_F_PT_NO_WRITE
                                                         | IEMTLBE_F_NO_MAPPINGR3   | fNoUser))
                          == pVCpu->iem.s.DataTlb.uTlbPhysRev))
            {
                /*
                 * Store the dword and return.
                 */
                STAM_STATS({pVCpu->iem.s.DataTlb.cTlbHits++;});
                Assert(pTlbe->pbMappingR3); /* (Only ever cleared by the owning EMT.) */
                Assert(!((uintptr_t)pTlbe->pbMappingR3 & GUEST_PAGE_OFFSET_MASK));
                *(TMPL_MEM_TYPE *)&pTlbe->pbMappingR3[GCPtrMem & GUEST_PAGE_OFFSET_MASK] = uValue;
                Log9(("IEM WR " TMPL_MEM_FMT_DESC " %RGv: " TMPL_MEM_FMT_TYPE "\n", GCPtrMem, uValue));
                return;
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


/*********************************************************************************************************************************
*   Mapping / Direct Memory Access                                                                                               *
*********************************************************************************************************************************/
# ifndef TMPL_MEM_NO_MAPPING

/**
 * Inlined read-write memory mapping function that longjumps on error.
 */
DECL_INLINE_THROW(TMPL_MEM_TYPE *)
RT_CONCAT3(iemMemMapData,TMPL_MEM_FN_SUFF,RwJmp)(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                 uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP
{
#  if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3) && !defined(TMPL_MEM_NO_INLINE)
    /*
     * Convert from segmented to flat address and check that it doesn't cross a page boundrary.
     */
    RTGCPTR GCPtrEff = iemMemApplySegmentToWriteJmp(pVCpu, iSegReg, sizeof(TMPL_MEM_TYPE), GCPtrMem);
#  if TMPL_MEM_TYPE_SIZE > 1
    if (RT_LIKELY(   !(GCPtrEff & TMPL_MEM_TYPE_ALIGN)
                  || TMPL_MEM_CHECK_UNALIGNED_WITHIN_PAGE_OK(pVCpu, GCPtrEff, TMPL_MEM_TYPE) ))
#  endif
    {
        /*
         * TLB lookup.
         */
        uint64_t const uTag  = IEMTLB_CALC_TAG(    &pVCpu->iem.s.DataTlb, GCPtrEff);
        PIEMTLBENTRY   pTlbe = IEMTLB_TAG_TO_ENTRY(&pVCpu->iem.s.DataTlb, uTag);
        if (RT_LIKELY(pTlbe->uTag == uTag))
        {
            /*
             * Check TLB page table level access flags.
             */
            AssertCompile(IEMTLBE_F_PT_NO_USER == 4);
            uint64_t const fNoUser = (IEM_GET_CPL(pVCpu) + 1) & IEMTLBE_F_PT_NO_USER;
            if (RT_LIKELY(   (pTlbe->fFlagsAndPhysRev & (  IEMTLBE_F_PHYS_REV       | IEMTLBE_F_NO_MAPPINGR3
                                                         | IEMTLBE_F_PG_UNASSIGNED  | IEMTLBE_F_PG_NO_WRITE   | IEMTLBE_F_PG_NO_READ
                                                         | IEMTLBE_F_PT_NO_ACCESSED | IEMTLBE_F_PT_NO_DIRTY   | IEMTLBE_F_PT_NO_WRITE
                                                         | fNoUser))
                          == pVCpu->iem.s.DataTlb.uTlbPhysRev))
            {
                /*
                 * Return the address.
                 */
                STAM_STATS({pVCpu->iem.s.DataTlb.cTlbHits++;});
                Assert(pTlbe->pbMappingR3); /* (Only ever cleared by the owning EMT.) */
                Assert(!((uintptr_t)pTlbe->pbMappingR3 & GUEST_PAGE_OFFSET_MASK));
                *pbUnmapInfo = 0;
                Log8(("IEM RW/map " TMPL_MEM_FMT_DESC " %d|%RGv: %p\n",
                      iSegReg, GCPtrMem, &pTlbe->pbMappingR3[GCPtrEff & GUEST_PAGE_OFFSET_MASK]));
                return (TMPL_MEM_TYPE *)&pTlbe->pbMappingR3[GCPtrEff & GUEST_PAGE_OFFSET_MASK];
            }
        }
    }

    /* Fall back on the slow careful approach in case of TLB miss, MMIO, exception
       outdated page pointer, or other troubles.  (This will do a TLB load.) */
    Log10Func(("%u:%RGv falling back\n", iSegReg, GCPtrMem));
#  endif
    return RT_CONCAT3(iemMemMapData,TMPL_MEM_FN_SUFF,RwSafeJmp)(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
}


/**
 * Inlined flat read-write memory mapping function that longjumps on error.
 */
DECL_INLINE_THROW(TMPL_MEM_TYPE *)
RT_CONCAT3(iemMemFlatMapData,TMPL_MEM_FN_SUFF,RwJmp)(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                     RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP
{
#  if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3) && !defined(TMPL_MEM_NO_INLINE)
    /*
     * Check that the address doesn't cross a page boundrary.
     */
#  if TMPL_MEM_TYPE_SIZE > 1
    if (RT_LIKELY(   !(GCPtrMem & TMPL_MEM_TYPE_ALIGN)
                  || TMPL_MEM_CHECK_UNALIGNED_WITHIN_PAGE_OK(pVCpu, GCPtrMem, TMPL_MEM_TYPE) ))
#  endif
    {
        /*
         * TLB lookup.
         */
        uint64_t const uTag  = IEMTLB_CALC_TAG(    &pVCpu->iem.s.DataTlb, GCPtrMem);
        PIEMTLBENTRY   pTlbe = IEMTLB_TAG_TO_ENTRY(&pVCpu->iem.s.DataTlb, uTag);
        if (RT_LIKELY(pTlbe->uTag == uTag))
        {
            /*
             * Check TLB page table level access flags.
             */
            AssertCompile(IEMTLBE_F_PT_NO_USER == 4);
            uint64_t const fNoUser = (IEM_GET_CPL(pVCpu) + 1) & IEMTLBE_F_PT_NO_USER;
            if (RT_LIKELY(   (pTlbe->fFlagsAndPhysRev & (  IEMTLBE_F_PHYS_REV       | IEMTLBE_F_NO_MAPPINGR3
                                                         | IEMTLBE_F_PG_UNASSIGNED  | IEMTLBE_F_PG_NO_WRITE   | IEMTLBE_F_PG_NO_READ
                                                         | IEMTLBE_F_PT_NO_ACCESSED | IEMTLBE_F_PT_NO_DIRTY   | IEMTLBE_F_PT_NO_WRITE
                                                         | fNoUser))
                          == pVCpu->iem.s.DataTlb.uTlbPhysRev))
            {
                /*
                 * Return the address.
                 */
                STAM_STATS({pVCpu->iem.s.DataTlb.cTlbHits++;});
                Assert(pTlbe->pbMappingR3); /* (Only ever cleared by the owning EMT.) */
                Assert(!((uintptr_t)pTlbe->pbMappingR3 & GUEST_PAGE_OFFSET_MASK));
                *pbUnmapInfo = 0;
                Log8(("IEM RW/map " TMPL_MEM_FMT_DESC " %RGv: %p\n",
                      GCPtrMem, &pTlbe->pbMappingR3[GCPtrMem & GUEST_PAGE_OFFSET_MASK]));
                return (TMPL_MEM_TYPE *)&pTlbe->pbMappingR3[GCPtrMem & GUEST_PAGE_OFFSET_MASK];
            }
        }
    }

    /* Fall back on the slow careful approach in case of TLB miss, MMIO, exception
       outdated page pointer, or other troubles.  (This will do a TLB load.) */
    Log10Func(("%RGv falling back\n", GCPtrMem));
#  endif
    return RT_CONCAT3(iemMemMapData,TMPL_MEM_FN_SUFF,RwSafeJmp)(pVCpu, pbUnmapInfo, UINT8_MAX, GCPtrMem);
}


/**
 * Inlined write-only memory mapping function that longjumps on error.
 */
DECL_INLINE_THROW(TMPL_MEM_TYPE *)
RT_CONCAT3(iemMemMapData,TMPL_MEM_FN_SUFF,WoJmp)(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                 uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP
{
#  if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3) && !defined(TMPL_MEM_NO_INLINE)
    /*
     * Convert from segmented to flat address and check that it doesn't cross a page boundrary.
     */
    RTGCPTR GCPtrEff = iemMemApplySegmentToWriteJmp(pVCpu, iSegReg, sizeof(TMPL_MEM_TYPE), GCPtrMem);
#  if TMPL_MEM_TYPE_SIZE > 1
    if (RT_LIKELY(   !(GCPtrEff & TMPL_MEM_TYPE_ALIGN)
                  || TMPL_MEM_CHECK_UNALIGNED_WITHIN_PAGE_OK(pVCpu, GCPtrEff, TMPL_MEM_TYPE) ))
#  endif
    {
        /*
         * TLB lookup.
         */
        uint64_t const uTag  = IEMTLB_CALC_TAG(    &pVCpu->iem.s.DataTlb, GCPtrEff);
        PIEMTLBENTRY   pTlbe = IEMTLB_TAG_TO_ENTRY(&pVCpu->iem.s.DataTlb, uTag);
        if (RT_LIKELY(pTlbe->uTag == uTag))
        {
            /*
             * Check TLB page table level access flags.
             */
            AssertCompile(IEMTLBE_F_PT_NO_USER == 4);
            uint64_t const fNoUser = (IEM_GET_CPL(pVCpu) + 1) & IEMTLBE_F_PT_NO_USER;
            if (RT_LIKELY(   (pTlbe->fFlagsAndPhysRev & (  IEMTLBE_F_PHYS_REV       | IEMTLBE_F_NO_MAPPINGR3
                                                         | IEMTLBE_F_PG_UNASSIGNED  | IEMTLBE_F_PG_NO_WRITE
                                                         | IEMTLBE_F_PT_NO_ACCESSED | IEMTLBE_F_PT_NO_DIRTY   | IEMTLBE_F_PT_NO_WRITE
                                                         | fNoUser))
                          == pVCpu->iem.s.DataTlb.uTlbPhysRev))
            {
                /*
                 * Return the address.
                 */
                STAM_STATS({pVCpu->iem.s.DataTlb.cTlbHits++;});
                Assert(pTlbe->pbMappingR3); /* (Only ever cleared by the owning EMT.) */
                Assert(!((uintptr_t)pTlbe->pbMappingR3 & GUEST_PAGE_OFFSET_MASK));
                *pbUnmapInfo = 0;
                Log8(("IEM WO/map " TMPL_MEM_FMT_DESC " %d|%RGv: %p\n",
                      iSegReg, GCPtrMem, &pTlbe->pbMappingR3[GCPtrEff & GUEST_PAGE_OFFSET_MASK]));
                return (TMPL_MEM_TYPE *)&pTlbe->pbMappingR3[GCPtrEff & GUEST_PAGE_OFFSET_MASK];
            }
        }
    }

    /* Fall back on the slow careful approach in case of TLB miss, MMIO, exception
       outdated page pointer, or other troubles.  (This will do a TLB load.) */
    Log10Func(("%u:%RGv falling back\n", iSegReg, GCPtrMem));
#  endif
    return RT_CONCAT3(iemMemMapData,TMPL_MEM_FN_SUFF,WoSafeJmp)(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
}


/**
 * Inlined flat write-only memory mapping function that longjumps on error.
 */
DECL_INLINE_THROW(TMPL_MEM_TYPE *)
RT_CONCAT3(iemMemFlatMapData,TMPL_MEM_FN_SUFF,WoJmp)(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                     RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP
{
#  if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3) && !defined(TMPL_MEM_NO_INLINE)
    /*
     * Check that the address doesn't cross a page boundrary.
     */
#  if TMPL_MEM_TYPE_SIZE > 1
    if (RT_LIKELY(   !(GCPtrMem & TMPL_MEM_TYPE_ALIGN)
                  || TMPL_MEM_CHECK_UNALIGNED_WITHIN_PAGE_OK(pVCpu, GCPtrMem, TMPL_MEM_TYPE) ))
#  endif
    {
        /*
         * TLB lookup.
         */
        uint64_t const uTag  = IEMTLB_CALC_TAG(    &pVCpu->iem.s.DataTlb, GCPtrMem);
        PIEMTLBENTRY   pTlbe = IEMTLB_TAG_TO_ENTRY(&pVCpu->iem.s.DataTlb, uTag);
        if (RT_LIKELY(pTlbe->uTag == uTag))
        {
            /*
             * Check TLB page table level access flags.
             */
            AssertCompile(IEMTLBE_F_PT_NO_USER == 4);
            uint64_t const fNoUser = (IEM_GET_CPL(pVCpu) + 1) & IEMTLBE_F_PT_NO_USER;
            if (RT_LIKELY(   (pTlbe->fFlagsAndPhysRev & (  IEMTLBE_F_PHYS_REV       | IEMTLBE_F_NO_MAPPINGR3
                                                         | IEMTLBE_F_PG_UNASSIGNED  | IEMTLBE_F_PG_NO_WRITE
                                                         | IEMTLBE_F_PT_NO_ACCESSED | IEMTLBE_F_PT_NO_DIRTY
                                                         | IEMTLBE_F_PT_NO_WRITE    | fNoUser))
                          == pVCpu->iem.s.DataTlb.uTlbPhysRev))
            {
                /*
                 * Return the address.
                 */
                STAM_STATS({pVCpu->iem.s.DataTlb.cTlbHits++;});
                Assert(pTlbe->pbMappingR3); /* (Only ever cleared by the owning EMT.) */
                Assert(!((uintptr_t)pTlbe->pbMappingR3 & GUEST_PAGE_OFFSET_MASK));
                *pbUnmapInfo = 0;
                Log8(("IEM WO/map " TMPL_MEM_FMT_DESC " %RGv: %p\n",
                      GCPtrMem, &pTlbe->pbMappingR3[GCPtrMem & GUEST_PAGE_OFFSET_MASK]));
                return (TMPL_MEM_TYPE *)&pTlbe->pbMappingR3[GCPtrMem & GUEST_PAGE_OFFSET_MASK];
            }
        }
    }

    /* Fall back on the slow careful approach in case of TLB miss, MMIO, exception
       outdated page pointer, or other troubles.  (This will do a TLB load.) */
    Log10Func(("%RGv falling back\n", GCPtrMem));
#  endif
    return RT_CONCAT3(iemMemMapData,TMPL_MEM_FN_SUFF,WoSafeJmp)(pVCpu, pbUnmapInfo, UINT8_MAX, GCPtrMem);
}


/**
 * Inlined read-only memory mapping function that longjumps on error.
 */
DECL_INLINE_THROW(TMPL_MEM_TYPE const *)
RT_CONCAT3(iemMemMapData,TMPL_MEM_FN_SUFF,RoJmp)(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                 uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP
{
#  if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3) && !defined(TMPL_MEM_NO_INLINE)
    /*
     * Convert from segmented to flat address and check that it doesn't cross a page boundrary.
     */
    RTGCPTR GCPtrEff = iemMemApplySegmentToReadJmp(pVCpu, iSegReg, sizeof(TMPL_MEM_TYPE), GCPtrMem);
#  if TMPL_MEM_TYPE_SIZE > 1
    if (RT_LIKELY(   !(GCPtrEff & TMPL_MEM_TYPE_ALIGN)
                  || TMPL_MEM_CHECK_UNALIGNED_WITHIN_PAGE_OK(pVCpu, GCPtrEff, TMPL_MEM_TYPE) ))
#  endif
    {
        /*
         * TLB lookup.
         */
        uint64_t const uTag  = IEMTLB_CALC_TAG(    &pVCpu->iem.s.DataTlb, GCPtrEff);
        PIEMTLBENTRY   pTlbe = IEMTLB_TAG_TO_ENTRY(&pVCpu->iem.s.DataTlb, uTag);
        if (RT_LIKELY(pTlbe->uTag == uTag))
        {
            /*
             * Check TLB page table level access flags.
             */
            AssertCompile(IEMTLBE_F_PT_NO_USER == 4);
            uint64_t const fNoUser = (IEM_GET_CPL(pVCpu) + 1) & IEMTLBE_F_PT_NO_USER;
            if (RT_LIKELY(   (pTlbe->fFlagsAndPhysRev & (  IEMTLBE_F_PHYS_REV       | IEMTLBE_F_NO_MAPPINGR3
                                                         | IEMTLBE_F_PG_UNASSIGNED  | IEMTLBE_F_PG_NO_READ
                                                         | IEMTLBE_F_PT_NO_ACCESSED | fNoUser))
                          == pVCpu->iem.s.DataTlb.uTlbPhysRev))
            {
                /*
                 * Return the address.
                 */
                STAM_STATS({pVCpu->iem.s.DataTlb.cTlbHits++;});
                Assert(pTlbe->pbMappingR3); /* (Only ever cleared by the owning EMT.) */
                Assert(!((uintptr_t)pTlbe->pbMappingR3 & GUEST_PAGE_OFFSET_MASK));
                *pbUnmapInfo = 0;
                Log9(("IEM RO/map " TMPL_MEM_FMT_DESC " %d|%RGv: %p\n",
                      iSegReg, GCPtrMem, &pTlbe->pbMappingR3[GCPtrEff & GUEST_PAGE_OFFSET_MASK]));
                return (TMPL_MEM_TYPE *)&pTlbe->pbMappingR3[GCPtrEff & GUEST_PAGE_OFFSET_MASK];
            }
        }
    }

    /* Fall back on the slow careful approach in case of TLB miss, MMIO, exception
       outdated page pointer, or other troubles.  (This will do a TLB load.) */
    Log10Func(("%u:%RGv falling back\n", iSegReg, GCPtrMem));
#  endif
    return RT_CONCAT3(iemMemMapData,TMPL_MEM_FN_SUFF,RoSafeJmp)(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
}


/**
 * Inlined read-only memory mapping function that longjumps on error.
 */
DECL_INLINE_THROW(TMPL_MEM_TYPE const *)
RT_CONCAT3(iemMemFlatMapData,TMPL_MEM_FN_SUFF,RoJmp)(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                     RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP
{
#  if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3) && !defined(TMPL_MEM_NO_INLINE)
    /*
     * Check that the address doesn't cross a page boundrary.
     */
#  if TMPL_MEM_TYPE_SIZE > 1
    if (RT_LIKELY(   !(GCPtrMem & TMPL_MEM_TYPE_ALIGN)
                  || TMPL_MEM_CHECK_UNALIGNED_WITHIN_PAGE_OK(pVCpu, GCPtrMem, TMPL_MEM_TYPE) ))
#  endif
    {
        /*
         * TLB lookup.
         */
        uint64_t const uTag  = IEMTLB_CALC_TAG(    &pVCpu->iem.s.DataTlb, GCPtrMem);
        PIEMTLBENTRY   pTlbe = IEMTLB_TAG_TO_ENTRY(&pVCpu->iem.s.DataTlb, uTag);
        if (RT_LIKELY(pTlbe->uTag == uTag))
        {
            /*
             * Check TLB page table level access flags.
             */
            AssertCompile(IEMTLBE_F_PT_NO_USER == 4);
            uint64_t const fNoUser = (IEM_GET_CPL(pVCpu) + 1) & IEMTLBE_F_PT_NO_USER;
            if (RT_LIKELY(   (pTlbe->fFlagsAndPhysRev & (  IEMTLBE_F_PHYS_REV       | IEMTLBE_F_NO_MAPPINGR3
                                                         | IEMTLBE_F_PG_UNASSIGNED  | IEMTLBE_F_PG_NO_READ
                                                         | IEMTLBE_F_PT_NO_ACCESSED | fNoUser))
                          == pVCpu->iem.s.DataTlb.uTlbPhysRev))
            {
                /*
                 * Return the address.
                 */
                STAM_STATS({pVCpu->iem.s.DataTlb.cTlbHits++;});
                Assert(pTlbe->pbMappingR3); /* (Only ever cleared by the owning EMT.) */
                Assert(!((uintptr_t)pTlbe->pbMappingR3 & GUEST_PAGE_OFFSET_MASK));
                *pbUnmapInfo = 0;
                Log9(("IEM RO/map " TMPL_MEM_FMT_DESC " %RGv: %p\n",
                      GCPtrMem, &pTlbe->pbMappingR3[GCPtrMem & GUEST_PAGE_OFFSET_MASK]));
                return (TMPL_MEM_TYPE const *)&pTlbe->pbMappingR3[GCPtrMem & GUEST_PAGE_OFFSET_MASK];
            }
        }
    }

    /* Fall back on the slow careful approach in case of TLB miss, MMIO, exception
       outdated page pointer, or other troubles.  (This will do a TLB load.) */
    Log10Func(("%RGv falling back\n", GCPtrMem));
#  endif
    return RT_CONCAT3(iemMemMapData,TMPL_MEM_FN_SUFF,RoSafeJmp)(pVCpu, pbUnmapInfo, UINT8_MAX, GCPtrMem);
}

# endif /* !TMPL_MEM_NO_MAPPING */


# ifdef TMPL_MEM_WITH_STACK
#  ifdef IEM_WITH_SETJMP

/**
 * Stack push function that longjmps on error.
 */
DECL_INLINE_THROW(void)
RT_CONCAT3(iemMemStackPush,TMPL_MEM_FN_SUFF,Jmp)(PVMCPUCC pVCpu, TMPL_MEM_TYPE uValue) IEM_NOEXCEPT_MAY_LONGJMP
{
#  if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3) && !defined(TMPL_MEM_NO_INLINE) && 1
    /*
     * Decrement the stack pointer (prep), apply segmentation and check that
     * the item doesn't cross a page boundrary.
     */
    uint64_t      uNewRsp;
    RTGCPTR const GCPtrTop = iemRegGetRspForPush(pVCpu, sizeof(TMPL_MEM_TYPE), &uNewRsp);
    RTGCPTR const GCPtrEff = iemMemApplySegmentToWriteJmp(pVCpu, X86_SREG_SS, sizeof(TMPL_MEM_TYPE), GCPtrTop);
#  if TMPL_MEM_TYPE_SIZE > 1
    if (RT_LIKELY(   !(GCPtrEff & TMPL_MEM_TYPE_ALIGN)
                  || TMPL_MEM_CHECK_UNALIGNED_WITHIN_PAGE_OK(pVCpu, GCPtrEff, TMPL_MEM_TYPE) ))
#  endif
    {
        /*
         * TLB lookup.
         */
        uint64_t const uTag  = IEMTLB_CALC_TAG(    &pVCpu->iem.s.DataTlb, GCPtrEff);
        PIEMTLBENTRY   pTlbe = IEMTLB_TAG_TO_ENTRY(&pVCpu->iem.s.DataTlb, uTag);
        if (RT_LIKELY(pTlbe->uTag == uTag))
        {
            /*
             * Check TLB page table level access flags.
             */
            AssertCompile(IEMTLBE_F_PT_NO_USER == 4);
            uint64_t const fNoUser = (IEM_GET_CPL(pVCpu) + 1) & IEMTLBE_F_PT_NO_USER;
            if (RT_LIKELY(   (pTlbe->fFlagsAndPhysRev & (  IEMTLBE_F_PHYS_REV       | IEMTLBE_F_NO_MAPPINGR3
                                                         | IEMTLBE_F_PG_UNASSIGNED  | IEMTLBE_F_PG_NO_WRITE
                                                         | IEMTLBE_F_PT_NO_ACCESSED | IEMTLBE_F_PT_NO_DIRTY
                                                         | IEMTLBE_F_PT_NO_WRITE    | fNoUser))
                          == pVCpu->iem.s.DataTlb.uTlbPhysRev))
            {
                /*
                 * Do the push and return.
                 */
                STAM_STATS({pVCpu->iem.s.DataTlb.cTlbHits++;});
                Assert(pTlbe->pbMappingR3); /* (Only ever cleared by the owning EMT.) */
                Assert(!((uintptr_t)pTlbe->pbMappingR3 & GUEST_PAGE_OFFSET_MASK));
                Log8(("IEM WR " TMPL_MEM_FMT_DESC " SS|%RGv (%RX64->%RX64): " TMPL_MEM_FMT_TYPE "\n",
                      GCPtrEff, pVCpu->cpum.GstCtx.rsp, uNewRsp, uValue));
                *(TMPL_MEM_TYPE *)&pTlbe->pbMappingR3[GCPtrEff & GUEST_PAGE_OFFSET_MASK] = uValue;
                pVCpu->cpum.GstCtx.rsp = uNewRsp;
                return;
            }
        }
    }

    /* Fall back on the slow careful approach in case of TLB miss, MMIO, exception
       outdated page pointer, or other troubles.  (This will do a TLB load.) */
    Log10Func(("%RGv falling back\n", GCPtrEff));
#  endif
    RT_CONCAT3(iemMemStackPush,TMPL_MEM_FN_SUFF,SafeJmp)(pVCpu, uValue);
}


/**
 * Stack pop function that longjmps on error.
 */
DECL_INLINE_THROW(TMPL_MEM_TYPE)
RT_CONCAT3(iemMemStackPop,TMPL_MEM_FN_SUFF,Jmp)(PVMCPUCC pVCpu) IEM_NOEXCEPT_MAY_LONGJMP
{
#  if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3) && !defined(TMPL_MEM_NO_INLINE) && 1
    /*
     * Increment the stack pointer (prep), apply segmentation and check that
     * the item doesn't cross a page boundrary.
     */
    uint64_t      uNewRsp;
    RTGCPTR const GCPtrTop = iemRegGetRspForPop(pVCpu, sizeof(TMPL_MEM_TYPE), &uNewRsp);
    RTGCPTR const GCPtrEff = iemMemApplySegmentToWriteJmp(pVCpu, X86_SREG_SS, sizeof(TMPL_MEM_TYPE), GCPtrTop);
#  if TMPL_MEM_TYPE_SIZE > 1
    if (RT_LIKELY(   !(GCPtrEff & TMPL_MEM_TYPE_ALIGN)
                  || TMPL_MEM_CHECK_UNALIGNED_WITHIN_PAGE_OK(pVCpu, GCPtrEff, TMPL_MEM_TYPE) ))
#  endif
    {
        /*
         * TLB lookup.
         */
        uint64_t const uTag  = IEMTLB_CALC_TAG(    &pVCpu->iem.s.DataTlb, GCPtrEff);
        PIEMTLBENTRY   pTlbe = IEMTLB_TAG_TO_ENTRY(&pVCpu->iem.s.DataTlb, uTag);
        if (RT_LIKELY(pTlbe->uTag == uTag))
        {
            /*
             * Check TLB page table level access flags.
             */
            AssertCompile(IEMTLBE_F_PT_NO_USER == 4);
            uint64_t const fNoUser = (IEM_GET_CPL(pVCpu) + 1) & IEMTLBE_F_PT_NO_USER;
            if (RT_LIKELY(   (pTlbe->fFlagsAndPhysRev & (  IEMTLBE_F_PHYS_REV       | IEMTLBE_F_NO_MAPPINGR3
                                                         | IEMTLBE_F_PG_UNASSIGNED  | IEMTLBE_F_PG_NO_READ
                                                         | IEMTLBE_F_PT_NO_ACCESSED | fNoUser))
                          == pVCpu->iem.s.DataTlb.uTlbPhysRev))
            {
                /*
                 * Do the push and return.
                 */
                STAM_STATS({pVCpu->iem.s.DataTlb.cTlbHits++;});
                Assert(pTlbe->pbMappingR3); /* (Only ever cleared by the owning EMT.) */
                Assert(!((uintptr_t)pTlbe->pbMappingR3 & GUEST_PAGE_OFFSET_MASK));
                TMPL_MEM_TYPE const uRet = *(TMPL_MEM_TYPE const *)&pTlbe->pbMappingR3[GCPtrEff & GUEST_PAGE_OFFSET_MASK];
                Log9(("IEM RD " TMPL_MEM_FMT_DESC " SS|%RGv (%RX64->%RX64): " TMPL_MEM_FMT_TYPE "\n",
                      GCPtrEff, pVCpu->cpum.GstCtx.rsp, uNewRsp, uRet));
                pVCpu->cpum.GstCtx.rsp = uNewRsp;
                return uRet;
            }
        }
    }

    /* Fall back on the slow careful approach in case of TLB miss, MMIO, exception
       outdated page pointer, or other troubles.  (This will do a TLB load.) */
    Log10Func(("%RGv falling back\n", GCPtrEff));
#  endif
    return RT_CONCAT3(iemMemStackPop,TMPL_MEM_FN_SUFF,SafeJmp)(pVCpu);
}

#   ifdef TMPL_WITH_PUSH_SREG
/**
 * Stack segment push function that longjmps on error.
 */
DECL_INLINE_THROW(void)
RT_CONCAT3(iemMemStackPush,TMPL_MEM_FN_SUFF,SRegJmp)(PVMCPUCC pVCpu, TMPL_MEM_TYPE uValue) IEM_NOEXCEPT_MAY_LONGJMP
{
    RT_CONCAT3(iemMemStackPush,TMPL_MEM_FN_SUFF,SRegSafeJmp)(pVCpu, uValue);
}

#   endif
#   if TMPL_MEM_TYPE_SIZE != 8

/**
 * 32-bit flat stack push function that longjmps on error.
 */
DECL_INLINE_THROW(void)
RT_CONCAT3(iemMemFlat32StackPush,TMPL_MEM_FN_SUFF,Jmp)(PVMCPUCC pVCpu, TMPL_MEM_TYPE uValue) IEM_NOEXCEPT_MAY_LONGJMP
{
    Assert(   pVCpu->cpum.GstCtx.ss.Attr.n.u1DefBig
           && pVCpu->cpum.GstCtx.ss.Attr.n.u4Type == X86_SEL_TYPE_RW_ACC
           && pVCpu->cpum.GstCtx.ss.u32Limit == UINT32_MAX
           && pVCpu->cpum.GstCtx.ss.u64Base == 0);
#  if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3) && !defined(TMPL_MEM_NO_INLINE) && 1
    /*
     * Calculate the new stack pointer and check that the item doesn't cross a page boundrary.
     */
    uint32_t const uNewEsp = pVCpu->cpum.GstCtx.esp - sizeof(TMPL_MEM_TYPE);
#  if TMPL_MEM_TYPE_SIZE > 1
    if (RT_LIKELY(   !(uNewEsp & TMPL_MEM_TYPE_ALIGN)
                  || TMPL_MEM_CHECK_UNALIGNED_WITHIN_PAGE_OK(pVCpu, uNewEsp, TMPL_MEM_TYPE) ))
#  endif
    {
        /*
         * TLB lookup.
         */
        uint64_t const uTag  = IEMTLB_CALC_TAG(    &pVCpu->iem.s.DataTlb, (RTGCPTR)uNewEsp); /* Doesn't work w/o casting to RTGCPTR (win /3 hangs). */
        PIEMTLBENTRY   pTlbe = IEMTLB_TAG_TO_ENTRY(&pVCpu->iem.s.DataTlb, uTag);
        if (RT_LIKELY(pTlbe->uTag == uTag))
        {
            /*
             * Check TLB page table level access flags.
             */
            AssertCompile(IEMTLBE_F_PT_NO_USER == 4);
            uint64_t const fNoUser = (IEM_GET_CPL(pVCpu) + 1) & IEMTLBE_F_PT_NO_USER;
            if (RT_LIKELY(   (pTlbe->fFlagsAndPhysRev & (  IEMTLBE_F_PHYS_REV       | IEMTLBE_F_NO_MAPPINGR3
                                                         | IEMTLBE_F_PG_UNASSIGNED  | IEMTLBE_F_PG_NO_WRITE
                                                         | IEMTLBE_F_PT_NO_ACCESSED | IEMTLBE_F_PT_NO_DIRTY
                                                         | IEMTLBE_F_PT_NO_WRITE    | fNoUser))
                          == pVCpu->iem.s.DataTlb.uTlbPhysRev))
            {
                /*
                 * Do the push and return.
                 */
                STAM_STATS({pVCpu->iem.s.DataTlb.cTlbHits++;});
                Assert(pTlbe->pbMappingR3); /* (Only ever cleared by the owning EMT.) */
                Assert(!((uintptr_t)pTlbe->pbMappingR3 & GUEST_PAGE_OFFSET_MASK));
                Log8(("IEM WR " TMPL_MEM_FMT_DESC " SS|%RX32 (<-%RX32): " TMPL_MEM_FMT_TYPE "\n",
                      uNewEsp, pVCpu->cpum.GstCtx.esp, uValue));
                *(TMPL_MEM_TYPE *)&pTlbe->pbMappingR3[uNewEsp & GUEST_PAGE_OFFSET_MASK] = uValue;
                pVCpu->cpum.GstCtx.rsp = uNewEsp;
                return;
            }
        }
    }

    /* Fall back on the slow careful approach in case of TLB miss, MMIO, exception
       outdated page pointer, or other troubles.  (This will do a TLB load.) */
    Log10Func(("%RX32 falling back\n", uNewEsp));
#  endif
    RT_CONCAT3(iemMemStackPush,TMPL_MEM_FN_SUFF,SafeJmp)(pVCpu, uValue);
}


/**
 * 32-bit flat stack pop function that longjmps on error.
 */
DECL_INLINE_THROW(TMPL_MEM_TYPE)
RT_CONCAT3(iemMemFlat32StackPop,TMPL_MEM_FN_SUFF,Jmp)(PVMCPUCC pVCpu) IEM_NOEXCEPT_MAY_LONGJMP
{
#  if defined(IEM_WITH_DATA_TLB) && defined(IN_RING3) && !defined(TMPL_MEM_NO_INLINE) && 1
    /*
     * Calculate the new stack pointer and check that the item doesn't cross a page boundrary.
     */
    uint32_t const uOldEsp = pVCpu->cpum.GstCtx.esp;
#  if TMPL_MEM_TYPE_SIZE > 1
    if (RT_LIKELY(   !(uOldEsp & TMPL_MEM_TYPE_ALIGN)
                  || TMPL_MEM_CHECK_UNALIGNED_WITHIN_PAGE_OK(pVCpu, uOldEsp, TMPL_MEM_TYPE) ))
#  endif
    {
        /*
         * TLB lookup.
         */
        uint64_t const uTag  = IEMTLB_CALC_TAG(    &pVCpu->iem.s.DataTlb, (RTGCPTR)uOldEsp); /* Cast is required! 2023-08-11 */
        PIEMTLBENTRY   pTlbe = IEMTLB_TAG_TO_ENTRY(&pVCpu->iem.s.DataTlb, uTag);
        if (RT_LIKELY(pTlbe->uTag == uTag))
        {
            /*
             * Check TLB page table level access flags.
             */
            AssertCompile(IEMTLBE_F_PT_NO_USER == 4);
            uint64_t const fNoUser = (IEM_GET_CPL(pVCpu) + 1) & IEMTLBE_F_PT_NO_USER;
            if (RT_LIKELY(   (pTlbe->fFlagsAndPhysRev & (  IEMTLBE_F_PHYS_REV       | IEMTLBE_F_NO_MAPPINGR3
                                                         | IEMTLBE_F_PG_UNASSIGNED  | IEMTLBE_F_PG_NO_READ
                                                         | IEMTLBE_F_PT_NO_ACCESSED | fNoUser))
                          == pVCpu->iem.s.DataTlb.uTlbPhysRev))
            {
                /*
                 * Do the push and return.
                 */
                STAM_STATS({pVCpu->iem.s.DataTlb.cTlbHits++;});
                Assert(pTlbe->pbMappingR3); /* (Only ever cleared by the owning EMT.) */
                Assert(!((uintptr_t)pTlbe->pbMappingR3 & GUEST_PAGE_OFFSET_MASK));
                TMPL_MEM_TYPE const uRet = *(TMPL_MEM_TYPE const *)&pTlbe->pbMappingR3[uOldEsp & GUEST_PAGE_OFFSET_MASK];
                pVCpu->cpum.GstCtx.rsp = uOldEsp + sizeof(TMPL_MEM_TYPE);
                Log9(("IEM RD " TMPL_MEM_FMT_DESC " SS|%RX32 (->%RX32): " TMPL_MEM_FMT_TYPE "\n",
                      uOldEsp, uOldEsp + sizeof(TMPL_MEM_TYPE), uRet));
                return uRet;
            }
        }
    }

    /* Fall back on the slow careful approach in case of TLB miss, MMIO, exception
       outdated page pointer, or other troubles.  (This will do a TLB load.) */
    Log10Func(("%RX32 falling back\n", uOldEsp));
#  endif
    return RT_CONCAT3(iemMemStackPop,TMPL_MEM_FN_SUFF,SafeJmp)(pVCpu);
}

#   endif /* TMPL_MEM_TYPE_SIZE != 8*/
#   ifdef TMPL_WITH_PUSH_SREG
/**
 * 32-bit flat stack segment push function that longjmps on error.
 */
DECL_INLINE_THROW(void)
RT_CONCAT3(iemMemFlat32StackPush,TMPL_MEM_FN_SUFF,SRegJmp)(PVMCPUCC pVCpu, TMPL_MEM_TYPE uValue) IEM_NOEXCEPT_MAY_LONGJMP
{
    RT_CONCAT3(iemMemStackPush,TMPL_MEM_FN_SUFF,SRegSafeJmp)(pVCpu, uValue);
}

#   endif
#   if TMPL_MEM_TYPE_SIZE != 4

/**
 * 64-bit flat stack push function that longjmps on error.
 */
DECL_INLINE_THROW(void)
RT_CONCAT3(iemMemFlat64StackPush,TMPL_MEM_FN_SUFF,Jmp)(PVMCPUCC pVCpu, TMPL_MEM_TYPE uValue) IEM_NOEXCEPT_MAY_LONGJMP
{
    RT_CONCAT3(iemMemStackPush,TMPL_MEM_FN_SUFF,SafeJmp)(pVCpu, uValue);
}


/**
 * 64-bit flat stack pop function that longjmps on error.
 */
DECL_INLINE_THROW(TMPL_MEM_TYPE)
RT_CONCAT3(iemMemFlat64StackPop,TMPL_MEM_FN_SUFF,Jmp)(PVMCPUCC pVCpu) IEM_NOEXCEPT_MAY_LONGJMP
{
    return RT_CONCAT3(iemMemStackPop,TMPL_MEM_FN_SUFF,SafeJmp)(pVCpu);
}

#endif /* TMPL_MEM_TYPE_SIZE != 4 */

#  endif /* IEM_WITH_SETJMP */
# endif /* TMPL_MEM_WITH_STACK */


#endif /* IEM_WITH_SETJMP */

#undef TMPL_MEM_TYPE
#undef TMPL_MEM_TYPE_ALIGN
#undef TMPL_MEM_TYPE_SIZE
#undef TMPL_MEM_FN_SUFF
#undef TMPL_MEM_FMT_TYPE
#undef TMPL_MEM_FMT_DESC
#undef TMPL_MEM_NO_STORE

