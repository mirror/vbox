/* $Id$ */
/** @file
 * PGM - Page Monitor, Guest Context.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_PGM
#include <VBox/pgm.h>
#include <VBox/cpum.h>
#include <VBox/mm.h>
#include <VBox/em.h>
#include <VBox/selm.h>
#include <VBox/iom.h>
#include <VBox/trpm.h>
#include <VBox/rem.h>
#include "PGMInternal.h"
#include <VBox/vm.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <VBox/log.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <VBox/dis.h>
#include <VBox/disopcode.h>



/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/



#ifndef RT_ARCH_AMD64
/*
 * Shadow - 32-bit mode
 */
#define PGM_SHW_TYPE                PGM_TYPE_32BIT
#define PGM_SHW_NAME(name)          PGM_SHW_NAME_32BIT(name)
#include "PGMGCShw.h"

/* Guest - real mode */
#define PGM_GST_TYPE                PGM_TYPE_REAL
#define PGM_GST_NAME(name)          PGM_GST_NAME_REAL(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_32BIT_REAL(name)
#include "PGMGCGst.h"
#include "PGMGCBth.h"
#undef PGM_BTH_NAME
#undef PGM_GST_TYPE
#undef PGM_GST_NAME

/* Guest - protected mode */
#define PGM_GST_TYPE                PGM_TYPE_PROT
#define PGM_GST_NAME(name)          PGM_GST_NAME_PROT(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_32BIT_PROT(name)
#include "PGMGCGst.h"
#include "PGMGCBth.h"
#undef PGM_BTH_NAME
#undef PGM_GST_TYPE
#undef PGM_GST_NAME

/* Guest - 32-bit mode */
#define PGM_GST_TYPE                PGM_TYPE_32BIT
#define PGM_GST_NAME(name)          PGM_GST_NAME_32BIT(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_32BIT_32BIT(name)
#include "PGMGCGst.h"
#include "PGMGCBth.h"
#undef PGM_BTH_NAME
#undef PGM_GST_TYPE
#undef PGM_GST_NAME

#undef PGM_SHW_TYPE
#undef PGM_SHW_NAME
#endif /* !RT_ARCH_AMD64 */


/*
 * Shadow - PAE mode
 */
#define PGM_SHW_TYPE                PGM_TYPE_PAE
#define PGM_SHW_NAME(name)          PGM_SHW_NAME_PAE(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_PAE_REAL(name)
#include "PGMGCShw.h"

/* Guest - real mode */
#define PGM_GST_TYPE                PGM_TYPE_REAL
#define PGM_GST_NAME(name)          PGM_GST_NAME_REAL(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_PAE_REAL(name)
#include "PGMGCBth.h"
#undef PGM_BTH_NAME
#undef PGM_GST_TYPE
#undef PGM_GST_NAME

/* Guest - protected mode */
#define PGM_GST_TYPE                PGM_TYPE_PROT
#define PGM_GST_NAME(name)          PGM_GST_NAME_PROT(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_PAE_PROT(name)
#include "PGMGCBth.h"
#undef PGM_BTH_NAME
#undef PGM_GST_TYPE
#undef PGM_GST_NAME

/* Guest - 32-bit mode */
#define PGM_GST_TYPE                PGM_TYPE_32BIT
#define PGM_GST_NAME(name)          PGM_GST_NAME_32BIT(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_PAE_32BIT(name)
#include "PGMGCBth.h"
#undef PGM_BTH_NAME
#undef PGM_GST_TYPE
#undef PGM_GST_NAME

/* Guest - PAE mode */
#define PGM_GST_TYPE                PGM_TYPE_PAE
#define PGM_GST_NAME(name)          PGM_GST_NAME_PAE(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_PAE_PAE(name)
#include "PGMGCGst.h"
#include "PGMGCBth.h"
#undef PGM_BTH_NAME
#undef PGM_GST_TYPE
#undef PGM_GST_NAME

#undef PGM_SHW_TYPE
#undef PGM_SHW_NAME


/*
 * Shadow - AMD64 mode
 */
#define PGM_SHW_TYPE                PGM_TYPE_AMD64
#define PGM_SHW_NAME(name)          PGM_SHW_NAME_AMD64(name)
#include "PGMGCShw.h"

/* Guest - AMD64 mode */
#define PGM_GST_TYPE                PGM_TYPE_AMD64
#define PGM_GST_NAME(name)          PGM_GST_NAME_AMD64(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_AMD64_AMD64(name)
#include "PGMGCGst.h"
#include "PGMGCBth.h"
#undef PGM_BTH_NAME
#undef PGM_GST_TYPE
#undef PGM_GST_NAME

#undef PGM_SHW_TYPE
#undef PGM_SHW_NAME


/**
 * Temporarily maps one guest page specified by GC physical address.
 * These pages must have a physical mapping in HC, i.e. they cannot be MMIO pages.
 *
 * Be WARNED that the dynamic page mapping area is small, 8 pages, thus the space is
 * reused after 8 mappings (or perhaps a few more if you score with the cache).
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   GCPhys      GC Physical address of the page.
 * @param   ppv         Where to store the address of the mapping.
 */
PGMGCDECL(int) PGMGCDynMapGCPage(PVM pVM, RTGCPHYS GCPhys, void **ppv)
{
    AssertMsg(!(GCPhys & PAGE_OFFSET_MASK), ("GCPhys=%VGp\n", GCPhys));

    /*
     * Get the ram range.
     */
    PPGMRAMRANGE pRam = pVM->pgm.s.pRamRangesGC;
    while (pRam && GCPhys - pRam->GCPhys >= pRam->cb)
        pRam = pRam->pNextGC;
    if (!pRam)
    {
        AssertMsgFailed(("Invalid physical address %VGp!\n", GCPhys));
        return VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS;
    }

    /*
     * Pass it on to PGMGCDynMapHCPage.
     */
    RTHCPHYS HCPhys = PGM_PAGE_GET_HCPHYS(&pRam->aPages[(GCPhys - pRam->GCPhys) >> PAGE_SHIFT]);
    //Log(("PGMGCDynMapGCPage: GCPhys=%VGp HCPhys=%VHp\n", GCPhys, HCPhys));
    return PGMGCDynMapHCPage(pVM, HCPhys, ppv);
}


/**
 * Temporarily maps one guest page specified by unaligned GC physical address.
 * These pages must have a physical mapping in HC, i.e. they cannot be MMIO pages.
 *
 * Be WARNED that the dynamic page mapping area is small, 8 pages, thus the space is
 * reused after 8 mappings (or perhaps a few more if you score with the cache).
 *
 * The caller is aware that only the speicifed page is mapped and that really bad things
 * will happen if writing beyond the page!
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   GCPhys      GC Physical address within the page to be mapped.
 * @param   ppv         Where to store the address of the mapping address corresponding to GCPhys.
 */
PGMGCDECL(int) PGMGCDynMapGCPageEx(PVM pVM, RTGCPHYS GCPhys, void **ppv)
{
    /*
     * Get the ram range.
     */
    PPGMRAMRANGE pRam = pVM->pgm.s.pRamRangesGC;
    while (pRam && GCPhys - pRam->GCPhys >= pRam->cb)
        pRam = pRam->pNextGC;
    if (!pRam)
    {
        AssertMsgFailed(("Invalid physical address %VGp!\n", GCPhys));
        return VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS;
    }

    /*
     * Pass it on to PGMGCDynMapHCPage.
     */
    RTHCPHYS HCPhys = PGM_PAGE_GET_HCPHYS(&pRam->aPages[(GCPhys - pRam->GCPhys) >> PAGE_SHIFT]);
    int rc = PGMGCDynMapHCPage(pVM, HCPhys, ppv);
    if (VBOX_SUCCESS(rc))
        *ppv = (void *)((uintptr_t)*ppv | (GCPhys & PAGE_OFFSET_MASK));
    return rc;
}


/**
 * Temporarily maps one host page specified by HC physical address.
 *
 * Be WARNED that the dynamic page mapping area is small, 8 pages, thus the space is
 * reused after 8 mappings (or perhaps a few more if you score with the cache).
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   HCPhys      HC Physical address of the page.
 * @param   ppv         Where to store the address of the mapping.
 */
PGMGCDECL(int) PGMGCDynMapHCPage(PVM pVM, RTHCPHYS HCPhys, void **ppv)
{
    AssertMsg(!(HCPhys & PAGE_OFFSET_MASK), ("HCPhys=%VHp\n", HCPhys));

    /*
     * Check the cache.
     */
    register unsigned iCache;
    if (    pVM->pgm.s.aHCPhysDynPageMapCache[iCache = 0] == HCPhys
        ||  pVM->pgm.s.aHCPhysDynPageMapCache[iCache = 1] == HCPhys
        ||  pVM->pgm.s.aHCPhysDynPageMapCache[iCache = 2] == HCPhys
        ||  pVM->pgm.s.aHCPhysDynPageMapCache[iCache = 3] == HCPhys)
    {
        static const uint8_t au8Trans[MM_HYPER_DYNAMIC_SIZE >> PAGE_SHIFT][ELEMENTS(pVM->pgm.s.aHCPhysDynPageMapCache)] =
        {
            { 0, 5, 6, 7 },
            { 0, 1, 6, 7 },
            { 0, 1, 2, 7 },
            { 0, 1, 2, 3 },
            { 4, 1, 2, 3 },
            { 4, 5, 2, 3 },
            { 4, 5, 6, 3 },
            { 4, 5, 6, 7 },
        };
        Assert(ELEMENTS(au8Trans) == 8);
        Assert(ELEMENTS(au8Trans[0]) == 4);
        int iPage = au8Trans[pVM->pgm.s.iDynPageMapLast][iCache];
        void *pv = pVM->pgm.s.pbDynPageMapBaseGC + (iPage << PAGE_SHIFT);
        *ppv = pv;
        STAM_COUNTER_INC(&pVM->pgm.s.StatDynMapCacheHits);
        //Log(("PGMGCDynMapHCPage: HCPhys=%VHp pv=%VGv iPage=%d iCache=%d\n", HCPhys, pv, iPage, iCache));
        return VINF_SUCCESS;
    }
    Assert(ELEMENTS(pVM->pgm.s.aHCPhysDynPageMapCache) == 4);
    STAM_COUNTER_INC(&pVM->pgm.s.StatDynMapCacheMisses);

    /*
     * Update the page tables.
     */
    register unsigned iPage = pVM->pgm.s.iDynPageMapLast;
    pVM->pgm.s.iDynPageMapLast = iPage = (iPage + 1) & ((MM_HYPER_DYNAMIC_SIZE >> PAGE_SHIFT) - 1);
    Assert((MM_HYPER_DYNAMIC_SIZE >> PAGE_SHIFT) == 8);

    pVM->pgm.s.aHCPhysDynPageMapCache[iPage & (ELEMENTS(pVM->pgm.s.aHCPhysDynPageMapCache) - 1)] = HCPhys;
    pVM->pgm.s.paDynPageMap32BitPTEsGC[iPage].u = (uint32_t)HCPhys | X86_PTE_P | X86_PTE_A | X86_PTE_D;
    pVM->pgm.s.paDynPageMapPaePTEsGC[iPage].u   =           HCPhys | X86_PTE_P | X86_PTE_A | X86_PTE_D;

    void *pv = pVM->pgm.s.pbDynPageMapBaseGC + (iPage << PAGE_SHIFT);
    *ppv = pv;
    ASMInvalidatePage(pv);
    Log4(("PGMGCDynMapHCPage: HCPhys=%VHp pv=%VGv iPage=%d\n", HCPhys, pv, iPage));
    return VINF_SUCCESS;
}


/**
 * Emulation of the invlpg instruction.
 *
 * @returns VBox status code suitable for scheduling.
 * @param   pVM         VM handle.
 * @param   GCPtrPage   Page to invalidate.
 */
PGMGCDECL(int) PGMGCInvalidatePage(PVM pVM, RTGCPTR GCPtrPage)
{
    LogFlow(("PGMGCInvalidatePage: GCPtrPage=%VGv\n", GCPtrPage));

    STAM_PROFILE_START(&CTXMID(pVM->pgm.s.Stat,InvalidatePage), a);

    /*
     * Check for conflicts and pending CR3 monitoring updates.
     */
    if (!pVM->pgm.s.fMappingsFixed)
    {
        if (    pgmGetMapping(pVM, GCPtrPage)
            /** @todo &&  (PGMGstGetPDE(pVM, GCPtrPage) & X86_PDE_P) - FIX THIS NOW!!! */ )
        {
            LogFlow(("PGMGCInvalidatePage: Conflict!\n"));
            VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
            STAM_PROFILE_STOP(&CTXMID(pVM->pgm.s.Stat,InvalidatePage), a);
            return VINF_PGM_SYNC_CR3;
        }

        if (pVM->pgm.s.fSyncFlags & PGM_SYNC_MONITOR_CR3)
        {
            LogFlow(("PGMGCInvalidatePage: PGM_SYNC_MONITOR_CR3 -> reinterpret instruction in HC\n"));
            STAM_PROFILE_STOP(&CTXMID(pVM->pgm.s.Stat,InvalidatePage), a);
            /** @todo counter for these... */
            return VINF_EM_RAW_EMULATE_INSTR;
        }
    }

    /*
     * Notify the recompiler so it can record this instruction.
     * Failure happens when it's out of space. We'll return to HC in that case.
     */
    int rc = REMNotifyInvalidatePage(pVM, GCPtrPage);
    if (rc == VINF_SUCCESS)
        rc = PGM_BTH_PFN(InvalidatePage, pVM)(pVM, GCPtrPage);

    STAM_PROFILE_STOP(&CTXMID(pVM->pgm.s.Stat,InvalidatePage), a);
    return rc;
}
