/* $Id$ */
/** @file
 * BS3Kit - Bs3PagingProtect
 */

/*
 * Copyright (C) 2007-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "bs3kit-template-header.h"
#include "bs3-cmn-paging.h"
#include <iprt/asm-amd64-x86.h>
#include <iprt/param.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#if 0
# define BS3PAGING_DPRINTF(a) Bs3TestPrintf a
#else
# define BS3PAGING_DPRINTF(a) do { } while (0)
#endif


static void *bs3PagingBuildPaeTable(uint64_t uTmpl, uint64_t cbIncrement, BS3MEMKIND enmKind, int *prc)
{
    uint64_t BS3_FAR *pau64 = (uint64_t BS3_FAR *)Bs3MemAlloc(enmKind, _4K);
    if (pau64)
    {
        unsigned i;
        for (i = 0; i < _4K / sizeof(uint64_t); i++, uTmpl += cbIncrement)
            pau64[i] = uTmpl;
    }
    else
        *prc = VERR_NO_MEMORY;
    return pau64;
}


BS3_DECL(X86PTE BS3_FAR *) bs3PagingGetLegacyPte(RTCCUINTXREG cr3, uint32_t uFlat, bool fUseInvlPg, int *prc)
{
    X86PTE BS3_FAR *pPTE = NULL;
#if TMPL_BITS == 16
    uint32_t const  uMaxAddr = BS3_MODE_IS_RM_OR_V86(g_bBs3CurrentMode) ? _1M - 1 : BS3_SEL_TILED_AREA_SIZE - 1;
#else
    uint32_t const  uMaxAddr = UINT32_MAX;
#endif
    BS3PAGING_DPRINTF(("bs3PagingGetLegacyPte: cr3=%RX32 uFlat=%RX32 uMaxAddr=%RX32\n", (uint32_t)cr3, uFlat, uMaxAddr));

    *prc = VERR_OUT_OF_RANGE;
    if (cr3 <= uMaxAddr)
    {
        unsigned const iPde = (uFlat >> X86_PD_SHIFT) & X86_PD_MASK;
        PX86PD const   pPD  = (PX86PD)Bs3XptrFlatToCurrent(cr3 & X86_CR3_PAGE_MASK);

        BS3_ASSERT(pPD->a[iPde].b.u1Present);
        if (pPD->a[iPde].b.u1Present)
        {
            unsigned const iPte = (uFlat >> X86_PT_SHIFT) & X86_PT_MASK;

            BS3_ASSERT(pPD->a[iPde].b.u1Present);
            BS3PAGING_DPRINTF(("bs3PagingGetLegacyPte: pPD=%p iPde=%#x: %#RX32\n", pPD, iPde, pPD->a[iPde]));
            if (pPD->a[iPde].b.u1Present)
            {
                if (!pPD->a[iPde].b.u1Size)
                {
                    if (pPD->a[iPde].u <= uMaxAddr)
                        pPTE = &((X86PT BS3_FAR *)Bs3XptrFlatToCurrent(pPD->a[iPde].u & ~(uint32_t)PAGE_OFFSET_MASK))->a[iPte];
                }
                else
                {
                    X86PT BS3_FAR *pPT;
                    uint32_t       uPte = (pPD->a[iPde].u & ~(uint32_t)(X86_PTE_PG_MASK | X86_PDE4M_PS | X86_PDE4M_G)) | X86_PTE_D;
                    if (pPD->a[iPde].b.u1Global)
                        uPte |= X86_PTE_G;
                    if (pPD->a[iPde].b.u1PAT)
                        uPte |= X86_PTE_PAT;

                    pPT = (X86PT BS3_FAR *)bs3PagingBuildPaeTable(RT_MAKE_U64(uPte, uPte + PAGE_SIZE),
                                                                  RT_MAKE_U64(PAGE_SIZE*2, PAGE_SIZE*2),
                                                                  uMaxAddr > _1M ? BS3MEMKIND_TILED : BS3MEMKIND_REAL, prc);

                    BS3PAGING_DPRINTF(("bs3PagingGetLegacyPte: Built pPT=%p uPte=%RX32\n", pPT, uPte));
                    if (pPT)
                    {
                        pPD->a[iPde].u = Bs3SelPtrToFlat(pPT)
                                       | (pPD->a[iPde].u & ~(uint32_t)(X86_PTE_PG_MASK | X86_PDE4M_PS | X86_PDE4M_G | X86_PDE4M_D));
                        BS3PAGING_DPRINTF(("bs3PagingGetLegacyPte: iPde=%#x: %#RX32\n", iPde, pPD->a[iPde].u));
                        if (fUseInvlPg)
                            ASMInvalidatePage(uFlat);
                        pPTE = &pPT->a[iPte];
                    }
                }
            }
        }
    }
    return pPTE;
}


BS3_DECL(X86PTEPAE BS3_FAR *) bs3PagingGetPte(RTCCUINTXREG cr3, uint64_t uFlat, bool fUseInvlPg, int *prc)
{
    X86PTEPAE BS3_FAR  *pPTE = NULL;
#if TMPL_BITS == 16
    uint32_t const      uMaxAddr = BS3_MODE_IS_RM_OR_V86(g_bBs3CurrentMode) ? _1M - 1 : BS3_SEL_TILED_AREA_SIZE - 1;
#else
    uintptr_t const     uMaxAddr = ~(uintptr_t)0;
#endif

    *prc = VERR_OUT_OF_RANGE;
    if ((cr3 & X86_CR3_AMD64_PAGE_MASK) <= uMaxAddr)
    {
        X86PDPAE BS3_FAR *pPD;
        if (BS3_MODE_IS_64BIT_SYS(g_bBs3CurrentMode))
        {
            unsigned const   iPml4e = (uFlat >> X86_PML4_SHIFT) & X86_PML4_MASK;
            X86PML4 BS3_FAR *pPml4  = (X86PML4 BS3_FAR *)Bs3XptrFlatToCurrent(cr3 & X86_CR3_AMD64_PAGE_MASK);
            BS3_ASSERT(pPml4->a[iPml4e].n.u1Present);
            if ((pPml4->a[iPml4e].u & X86_PML4E_PG_MASK) <= uMaxAddr)
            {
                unsigned const   iPdpte = (uFlat >> X86_PDPT_SHIFT) & X86_PDPT_MASK_AMD64;
                X86PDPT BS3_FAR *pPdpt  = (X86PDPT BS3_FAR *)Bs3XptrFlatToCurrent(pPml4->a[iPml4e].u & X86_PML4E_PG_MASK);
                BS3_ASSERT(pPdpt->a[iPdpte].n.u1Present);
                if (!pPdpt->a[iPdpte].b.u1Size)
                {
                    if ((pPdpt->a[iPdpte].u & X86_PDPE_PG_MASK) <= uMaxAddr)
                        pPD = (X86PDPAE BS3_FAR *)Bs3XptrFlatToCurrent(pPdpt->a[iPdpte].u & ~(uint64_t)PAGE_OFFSET_MASK);
                }
                else
                {
                    /* Split 1GB page. */
                    pPD = (X86PDPAE BS3_FAR *)bs3PagingBuildPaeTable(pPdpt->a[iPdpte].u, _2M,
                                                                     uMaxAddr > _1M ? BS3MEMKIND_TILED : BS3MEMKIND_REAL, prc);
                    if (pPD)
                    {
                        pPdpt->a[iPdpte].u = Bs3SelPtrToFlat(pPD)
                                           | (  pPdpt->a[iPdpte].u
                                              & ~(uint64_t)(X86_PDPE_PG_MASK | X86_PDE4M_PS | X86_PDE4M_G | X86_PDE4M_D));
                        if (fUseInvlPg)
                            ASMInvalidatePage(uFlat);
                    }
                }
            }
        }
        //else if (uFlat <= UINT32_MAX) - fixme!
        else if (!(uFlat >> 32))
        {
            unsigned const   iPdpte = ((uint32_t)uFlat >> X86_PDPT_SHIFT) & X86_PDPT_MASK_PAE;
            X86PDPT BS3_FAR *pPdpt  = (X86PDPT BS3_FAR *)Bs3XptrFlatToCurrent(cr3 & X86_CR3_PAE_PAGE_MASK);
            BS3_ASSERT(pPdpt->a[iPdpte].n.u1Present);
            if ((pPdpt->a[iPdpte].u & X86_PDPE_PG_MASK) <= uMaxAddr)
                pPD = (X86PDPAE BS3_FAR *)Bs3XptrFlatToCurrent(pPdpt->a[iPdpte].u & X86_PDPE_PG_MASK);
        }
        else
            pPD = NULL;
        if (pPD)
        {
            unsigned const iPte = (uFlat >> X86_PT_PAE_SHIFT) & X86_PT_PAE_MASK;
            unsigned const iPde = (uFlat >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK;
            if (!pPD->a[iPde].b.u1Size)
            {
                if ((pPD->a[iPde].u & X86_PDE_PAE_PG_MASK) <= uMaxAddr)
                    pPTE = &((X86PTPAE BS3_FAR *)Bs3XptrFlatToCurrent(pPD->a[iPde].u & ~(uint64_t)PAGE_OFFSET_MASK))->a[iPte];
            }
            else
            {
                /* Split 2MB page. */
                X86PTPAE BS3_FAR *pPT;
                uint64_t uTmpl = pPD->a[iPde].u & ~(uint64_t)(X86_PDE4M_G | X86_PDE4M_PS | X86_PDE4M_PAT);
                if (!pPD->a[iPde].b.u1Global)
                    uTmpl |= X86_PTE_G;
                if (!pPD->a[iPde].b.u1PAT)
                    uTmpl |= X86_PTE_PAT;

                pPT = (X86PTPAE BS3_FAR *)bs3PagingBuildPaeTable(uTmpl, PAGE_SIZE,
                                                                 uMaxAddr > _1M ? BS3MEMKIND_TILED : BS3MEMKIND_REAL, prc);
                if (pPT)
                {
                    pPD->a[iPde].u = Bs3SelPtrToFlat(pPT)
                                   | (pPD->a[iPde].u & ~(uint64_t)(X86_PTE_PAE_PG_MASK | X86_PDE4M_PS | X86_PDE4M_G | X86_PDE4M_D));
                    if (fUseInvlPg)
                        ASMInvalidatePage(uFlat);
                    pPTE = &pPT->a[iPte];
                }
            }
        }
    }

    return pPTE;
}


BS3_DECL(int) Bs3PagingProtect(uint64_t uFlat, uint64_t cb, uint64_t fSet, uint64_t fClear)
{
    RTCCUINTXREG const  cr3        = ASMGetCR3();
    RTCCUINTXREG const  cr4        = ASMGetCR4();
    bool const          fLegacyPTs = !(cr4 & X86_CR4_PAE);
    bool const          fUseInvlPg = (g_uBs3CpuDetected & BS3CPU_TYPE_MASK) >= BS3CPU_80486
                                  && (   cb < UINT64_C(16)*PAGE_SIZE
                                      || (cr4 & X86_CR4_PGE));
    unsigned            cEntries;
    int                 rc;

    BS3PAGING_DPRINTF(("Bs3PagingProtect: reloading cr3=%RX32\n", (uint32_t)cr3));
    ASMSetCR3(cr3);

    /*
     * Adjust the range parameters.
     */
    cb += uFlat & PAGE_OFFSET_MASK;
    cb = RT_ALIGN_64(cb, PAGE_SIZE);
    uFlat &= ~(uint64_t)PAGE_OFFSET_MASK;

    fSet   &= ~X86_PTE_PAE_PG_MASK;
    fClear &= ~X86_PTE_PAE_PG_MASK;

    BS3PAGING_DPRINTF(("Bs3PagingProtect: uFlat=%RX64 cb=%RX64 fSet=%RX64 fClear=%RX64 %s %s\n", uFlat, cb, fSet, fClear,
                       fLegacyPTs ? "legacy" : "pae/amd64", fUseInvlPg ? "invlpg" : "reload-cr3"));
    if (fLegacyPTs)
    {
        /*
         * Legacy page tables.
         */
        while ((uint32_t)cb > 0)
        {
            PX86PTE pPte = bs3PagingGetLegacyPte(cr3, (uint32_t)uFlat, fUseInvlPg, &rc);
            if (!pPte)
                return rc;

            cEntries = X86_PG_ENTRIES - ((uFlat >> X86_PT_SHIFT) & X86_PT_MASK);
            while (cEntries-- > 0 && cb > 0)
            {
                pPte->u &= ~(uint32_t)fClear;
                pPte->u |= (uint32_t)fSet;
                if (fUseInvlPg)
                    ASMInvalidatePage(uFlat);

                pPte++;
                uFlat += PAGE_SIZE;
                cb    -= PAGE_SIZE;
            }
        }
    }
    else
    {
        /*
         * Long mode or PAE page tables (at this level they are the same).
         */
        while (cb > 0)
        {
            PX86PTEPAE pPte = bs3PagingGetPte(cr3, uFlat, fUseInvlPg, &rc);
            if (!pPte)
                return rc;

            cEntries = X86_PG_ENTRIES - ((uFlat >> X86_PT_PAE_SHIFT) & X86_PT_PAE_MASK);
            while (cEntries-- > 0 && cb > 0)
            {
                pPte->u &= ~fClear;
                pPte->u |= fSet;
                if (fUseInvlPg)
                    ASMInvalidatePage(uFlat);

                pPte++;
                uFlat += PAGE_SIZE;
                cb    -= PAGE_SIZE;
            }
        }
    }

    /*
     * Flush the TLB if we didn't use INVLPG above.
     */
    BS3PAGING_DPRINTF(("Bs3PagingProtect: reloading cr3=%RX32\n", (uint32_t)cr3));
    //if (!fUseInvlPg)
        ASMSetCR3(cr3);
    BS3PAGING_DPRINTF(("Bs3PagingProtect: reloaded cr3=%RX32\n", (uint32_t)cr3));

    return VINF_SUCCESS;
}


BS3_DECL(int) Bs3PagingProtectPtr(void *pv, size_t cb, uint64_t fSet, uint64_t fClear)
{
#if ARCH_BITS == 16
    return Bs3PagingProtect(Bs3SelPtrToFlat(pv), cb, fSet, fClear);
#else
    return Bs3PagingProtect((uintptr_t)pv, cb, fSet, fClear);
#endif
}

