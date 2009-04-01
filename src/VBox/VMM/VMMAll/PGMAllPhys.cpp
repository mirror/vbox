/* $Id$ */
/** @file
 * PGM - Page Manager and Monitor, Physical Memory Addressing.
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
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** @def PGM_IGNORE_RAM_FLAGS_RESERVED
 * Don't respect the MM_RAM_FLAGS_RESERVED flag when converting to HC addresses.
 *
 * Since this flag is currently incorrectly kept set for ROM regions we will
 * have to ignore it for now so we don't break stuff.
 *
 * @todo this has been fixed now I believe, remove this hack.
 */
#define PGM_IGNORE_RAM_FLAGS_RESERVED


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_PGM_PHYS
#include <VBox/pgm.h>
#include <VBox/trpm.h>
#include <VBox/vmm.h>
#include <VBox/iom.h>
#include <VBox/em.h>
#include <VBox/rem.h>
#include "PGMInternal.h"
#include <VBox/vm.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/asm.h>
#include <VBox/log.h>
#ifdef IN_RING3
# include <iprt/thread.h>
#endif



#ifndef IN_RING3

/**
 * \#PF Handler callback for Guest ROM range write access.
 * We simply ignore the writes or fall back to the recompiler if we don't support the instruction.
 *
 * @returns VBox status code (appropritate for trap handling and GC return).
 * @param   pVM         VM Handle.
 * @param   uErrorCode  CPU Error code.
 * @param   pRegFrame   Trap register frame.
 * @param   pvFault     The fault address (cr2).
 * @param   GCPhysFault The GC physical address corresponding to pvFault.
 * @param   pvUser      User argument. Pointer to the ROM range structure.
 */
VMMDECL(int) pgmPhysRomWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPHYS GCPhysFault, void *pvUser)
{
    int             rc;
#ifdef VBOX_WITH_NEW_PHYS_CODE
    PPGMROMRANGE    pRom = (PPGMROMRANGE)pvUser;
    uint32_t        iPage = (GCPhysFault - pRom->GCPhys) >> PAGE_SHIFT;
    Assert(iPage < (pRom->cb >> PAGE_SHIFT));
    switch (pRom->aPages[iPage].enmProt)
    {
        case PGMROMPROT_READ_ROM_WRITE_IGNORE:
        case PGMROMPROT_READ_RAM_WRITE_IGNORE:
        {
#endif
            /*
             * If it's a simple instruction which doesn't change the cpu state
             * we will simply skip it. Otherwise we'll have to defer it to REM.
             */
            uint32_t cbOp;
            DISCPUSTATE Cpu;
            rc = EMInterpretDisasOne(pVM, pRegFrame, &Cpu, &cbOp);
            if (     RT_SUCCESS(rc)
                &&   Cpu.mode == CPUMODE_32BIT  /** @todo why does this matter? */
                &&  !(Cpu.prefix & (PREFIX_REPNE | PREFIX_REP | PREFIX_SEG)))
            {
                switch (Cpu.opcode)
                {
                    /** @todo Find other instructions we can safely skip, possibly
                     * adding this kind of detection to DIS or EM. */
                    case OP_MOV:
                        pRegFrame->rip += cbOp;
                        STAM_COUNTER_INC(&pVM->pgm.s.StatRZGuestROMWriteHandled);
                        return VINF_SUCCESS;
                }
            }
            else if (RT_UNLIKELY(rc == VERR_INTERNAL_ERROR))
                return rc;
#ifdef VBOX_WITH_NEW_PHYS_CODE
            break;
        }

        case PGMROMPROT_READ_RAM_WRITE_RAM:
            rc = PGMHandlerPhysicalPageTempOff(pVM, pRom->GCPhys, GCPhysFault & X86_PTE_PG_MASK);
            AssertRC(rc);
            break; /** @todo Must edit the shadow PT and restart the instruction, not use the interpreter! */

        case PGMROMPROT_READ_ROM_WRITE_RAM:
            /* Handle it in ring-3 because it's *way* easier there. */
            break;

        default:
            AssertMsgFailedReturn(("enmProt=%d iPage=%d GCPhysFault=%RGp\n",
                                   pRom->aPages[iPage].enmProt, iPage, GCPhysFault),
                                  VERR_INTERNAL_ERROR);
    }
#endif

    STAM_COUNTER_INC(&pVM->pgm.s.StatRZGuestROMWriteUnhandled);
    return VINF_EM_RAW_EMULATE_INSTR;
}

#endif /* IN_RING3 */

/**
 * Checks if Address Gate 20 is enabled or not.
 *
 * @returns true if enabled.
 * @returns false if disabled.
 * @param   pVM     VM handle.
 */
VMMDECL(bool) PGMPhysIsA20Enabled(PVM pVM)
{
    LogFlow(("PGMPhysIsA20Enabled %d\n", pVM->pgm.s.fA20Enabled));
    return pVM->pgm.s.fA20Enabled;
}


/**
 * Validates a GC physical address.
 *
 * @returns true if valid.
 * @returns false if invalid.
 * @param   pVM     The VM handle.
 * @param   GCPhys  The physical address to validate.
 */
VMMDECL(bool) PGMPhysIsGCPhysValid(PVM pVM, RTGCPHYS GCPhys)
{
    PPGMPAGE pPage = pgmPhysGetPage(&pVM->pgm.s, GCPhys);
    return pPage != NULL;
}


/**
 * Checks if a GC physical address is a normal page,
 * i.e. not ROM, MMIO or reserved.
 *
 * @returns true if normal.
 * @returns false if invalid, ROM, MMIO or reserved page.
 * @param   pVM     The VM handle.
 * @param   GCPhys  The physical address to check.
 */
VMMDECL(bool) PGMPhysIsGCPhysNormal(PVM pVM, RTGCPHYS GCPhys)
{
    PPGMPAGE pPage = pgmPhysGetPage(&pVM->pgm.s, GCPhys);
#ifdef VBOX_WITH_NEW_PHYS_CODE
    return pPage
        && PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_RAM;
#else
    return pPage
        && !(pPage->HCPhys & (MM_RAM_FLAGS_MMIO | MM_RAM_FLAGS_ROM | MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_MMIO2));
#endif
}


/**
 * Converts a GC physical address to a HC physical address.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_PGM_PHYS_PAGE_RESERVED it it's a valid GC physical
 *          page but has no physical backing.
 * @returns VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid
 *          GC physical address.
 *
 * @param   pVM     The VM handle.
 * @param   GCPhys  The GC physical address to convert.
 * @param   pHCPhys Where to store the HC physical address on success.
 */
VMMDECL(int) PGMPhysGCPhys2HCPhys(PVM pVM, RTGCPHYS GCPhys, PRTHCPHYS pHCPhys)
{
    PPGMPAGE pPage;
    int rc = pgmPhysGetPageEx(&pVM->pgm.s, GCPhys, &pPage);
    if (RT_FAILURE(rc))
        return rc;

#ifndef PGM_IGNORE_RAM_FLAGS_RESERVED
    if (RT_UNLIKELY(pPage->HCPhys & MM_RAM_FLAGS_RESERVED)) /** @todo PAGE FLAGS */
        return VERR_PGM_PHYS_PAGE_RESERVED;
#endif

    *pHCPhys = PGM_PAGE_GET_HCPHYS(pPage) | (GCPhys & PAGE_OFFSET_MASK);
    return VINF_SUCCESS;
}


/**
 * Invalidates the GC page mapping TLB.
 *
 * @param   pVM     The VM handle.
 */
VMMDECL(void) PGMPhysInvalidatePageGCMapTLB(PVM pVM)
{
    /* later */
    NOREF(pVM);
}


/**
 * Invalidates the ring-0 page mapping TLB.
 *
 * @param   pVM     The VM handle.
 */
VMMDECL(void) PGMPhysInvalidatePageR0MapTLB(PVM pVM)
{
    PGMPhysInvalidatePageR3MapTLB(pVM);
}


/**
 * Invalidates the ring-3 page mapping TLB.
 *
 * @param   pVM     The VM handle.
 */
VMMDECL(void) PGMPhysInvalidatePageR3MapTLB(PVM pVM)
{
    pgmLock(pVM);
    for (unsigned i = 0; i < RT_ELEMENTS(pVM->pgm.s.PhysTlbHC.aEntries); i++)
    {
        pVM->pgm.s.PhysTlbHC.aEntries[i].GCPhys = NIL_RTGCPHYS;
        pVM->pgm.s.PhysTlbHC.aEntries[i].pPage = 0;
        pVM->pgm.s.PhysTlbHC.aEntries[i].pMap = 0;
        pVM->pgm.s.PhysTlbHC.aEntries[i].pv = 0;
    }
    pgmUnlock(pVM);
}


/**
 * Makes sure that there is at least one handy page ready for use.
 *
 * This will also take the appropriate actions when reaching water-marks.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_EM_NO_MEMORY if we're really out of memory.
 *
 * @param   pVM     The VM handle.
 *
 * @remarks Must be called from within the PGM critical section. It may
 *          nip back to ring-3/0 in some cases.
 */
static int pgmPhysEnsureHandyPage(PVM pVM)
{
    AssertMsg(pVM->pgm.s.cHandyPages <= RT_ELEMENTS(pVM->pgm.s.aHandyPages), ("%d\n", pVM->pgm.s.cHandyPages));

    /*
     * Do we need to do anything special?
     */
#ifdef IN_RING3
    if (pVM->pgm.s.cHandyPages <= RT_MAX(PGM_HANDY_PAGES_SET_FF, PGM_HANDY_PAGES_R3_ALLOC))
#else
    if (pVM->pgm.s.cHandyPages <= RT_MAX(PGM_HANDY_PAGES_SET_FF, PGM_HANDY_PAGES_RZ_TO_R3))
#endif
    {
        /*
         * Allocate pages only if we're out of them, or in ring-3, almost out.
         */
#ifdef IN_RING3
        if (pVM->pgm.s.cHandyPages <= PGM_HANDY_PAGES_R3_ALLOC)
#else
        if (pVM->pgm.s.cHandyPages <= PGM_HANDY_PAGES_RZ_ALLOC)
#endif
        {
            Log(("PGM: cHandyPages=%u out of %u -> allocate more; VM_FF_PGM_NO_MEMORY=%RTbool\n",
                 pVM->pgm.s.cHandyPages, RT_ELEMENTS(pVM->pgm.s.aHandyPages), VM_FF_ISSET(pVM, VM_FF_PGM_NO_MEMORY) ));
#ifdef IN_RING3
            int rc = PGMR3PhysAllocateHandyPages(pVM);
#elif defined(IN_RING0)
            int rc = VMMR0CallHost(pVM, VMMCALLHOST_PGM_ALLOCATE_HANDY_PAGES, 0);
#else
            int rc = VMMGCCallHost(pVM, VMMCALLHOST_PGM_ALLOCATE_HANDY_PAGES, 0);
#endif
            if (RT_UNLIKELY(rc != VINF_SUCCESS))
            {
                if (RT_FAILURE(rc))
                    return rc;
                AssertMsgReturn(rc == VINF_EM_NO_MEMORY, ("%Rrc\n", rc), VERR_IPE_UNEXPECTED_INFO_STATUS);
                if (!pVM->pgm.s.cHandyPages)
                {
                    LogRel(("PGM: no more handy pages!\n"));
                    return VERR_EM_NO_MEMORY;
                }
                Assert(VM_FF_ISSET(pVM, VM_FF_PGM_NEED_HANDY_PAGES));
                Assert(VM_FF_ISSET(pVM, VM_FF_PGM_NO_MEMORY));
#ifdef IN_RING3
                REMR3NotifyFF(pVM);
#else
                VM_FF_SET(pVM, VM_FF_TO_R3); /* paranoia */
#endif
            }
            AssertMsgReturn(    pVM->pgm.s.cHandyPages > 0
                            &&  pVM->pgm.s.cHandyPages <= RT_ELEMENTS(pVM->pgm.s.aHandyPages),
                            ("%u\n", pVM->pgm.s.cHandyPages),
                            VERR_INTERNAL_ERROR);
        }
        else
        {
            if (pVM->pgm.s.cHandyPages <= PGM_HANDY_PAGES_SET_FF)
                VM_FF_SET(pVM, VM_FF_PGM_NEED_HANDY_PAGES);
#ifndef IN_RING3
            if (pVM->pgm.s.cHandyPages <= PGM_HANDY_PAGES_RZ_TO_R3)
            {
                Log(("PGM: VM_FF_TO_R3 - cHandyPages=%u out of %u\n", pVM->pgm.s.cHandyPages, RT_ELEMENTS(pVM->pgm.s.aHandyPages)));
                VM_FF_SET(pVM, VM_FF_TO_R3);
            }
#endif
        }
    }

    return VINF_SUCCESS;
}


/**
 * Replace a zero or shared page with new page that we can write to.
 *
 * @returns The following VBox status codes.
 * @retval  VINF_SUCCESS on success, pPage is modified.
 * @retval  VINF_PGM_SYNC_CR3 on success and a page pool flush is pending.
 * @retval  VERR_EM_NO_MEMORY if we're totally out of memory.
 *
 * @todo    Propagate VERR_EM_NO_MEMORY up the call tree.
 *
 * @param   pVM         The VM address.
 * @param   pPage       The physical page tracking structure. This will
 *                      be modified on success.
 * @param   GCPhys      The address of the page.
 *
 * @remarks Must be called from within the PGM critical section. It may
 *          nip back to ring-3/0 in some cases.
 *
 * @remarks This function shouldn't really fail, however if it does
 *          it probably means we've screwed up the size of the amount
 *          and/or the low-water mark of handy pages. Or, that some
 *          device I/O is causing a lot of pages to be allocated while
 *          while the host is in a low-memory condition.
 */
int pgmPhysAllocPage(PVM pVM, PPGMPAGE pPage, RTGCPHYS GCPhys)
{
    LogFlow(("pgmPhysAllocPage: %R[pgmpage] %RGp\n", pPage, GCPhys));

    /*
     * Prereqs.
     */
    Assert(PDMCritSectIsOwner(&pVM->pgm.s.CritSect));
    AssertMsg(PGM_PAGE_IS_ZERO(pPage) || PGM_PAGE_IS_SHARED(pPage), ("%R[pgmpage] %RGp\n", pPage, GCPhys));
    Assert(!PGM_PAGE_IS_MMIO(pPage));


    /*
     * Flush any shadow page table mappings of the page.
     * When VBOX_WITH_NEW_LAZY_PAGE_ALLOC isn't defined, there shouldn't be any.
     */
    bool fFlushTLBs = false;
    int rc = pgmPoolTrackFlushGCPhys(pVM, pPage, &fFlushTLBs);
    AssertMsgReturn(rc == VINF_SUCCESS || rc == VINF_PGM_SYNC_CR3, ("%Rrc\n", rc), RT_FAILURE(rc) ? rc : VERR_IPE_UNEXPECTED_STATUS);

    /*
     * Ensure that we've got a page handy, take it and use it.
     */
    int rc2 = pgmPhysEnsureHandyPage(pVM);
    if (RT_FAILURE(rc2))
    {
        if (fFlushTLBs)
            PGM_INVL_GUEST_TLBS();
        Assert(rc2 == VERR_EM_NO_MEMORY);
        return rc2;
    }
    /* re-assert preconditions since pgmPhysEnsureHandyPage may do a context switch. */
    Assert(PDMCritSectIsOwner(&pVM->pgm.s.CritSect));
    AssertMsg(PGM_PAGE_IS_ZERO(pPage) || PGM_PAGE_IS_SHARED(pPage), ("%R[pgmpage] %RGp\n", pPage, GCPhys));
    Assert(!PGM_PAGE_IS_MMIO(pPage));

    uint32_t iHandyPage = --pVM->pgm.s.cHandyPages;
    AssertMsg(iHandyPage < RT_ELEMENTS(pVM->pgm.s.aHandyPages), ("%d\n", iHandyPage));
    Assert(pVM->pgm.s.aHandyPages[iHandyPage].HCPhysGCPhys != NIL_RTHCPHYS);
    Assert(!(pVM->pgm.s.aHandyPages[iHandyPage].HCPhysGCPhys & ~X86_PTE_PAE_PG_MASK));
    Assert(pVM->pgm.s.aHandyPages[iHandyPage].idPage != NIL_GMM_PAGEID);
    Assert(pVM->pgm.s.aHandyPages[iHandyPage].idSharedPage == NIL_GMM_PAGEID);

    /*
     * There are one or two action to be taken the next time we allocate handy pages:
     *      - Tell the GMM (global memory manager) what the page is being used for.
     *        (Speeds up replacement operations - sharing and defragmenting.)
     *      - If the current backing is shared, it must be freed.
     */
    const RTHCPHYS HCPhys = pVM->pgm.s.aHandyPages[iHandyPage].HCPhysGCPhys;
    pVM->pgm.s.aHandyPages[iHandyPage].HCPhysGCPhys = GCPhys & ~(RTGCPHYS)PAGE_OFFSET_MASK;

    if (PGM_PAGE_IS_SHARED(pPage))
    {
        pVM->pgm.s.aHandyPages[iHandyPage].idSharedPage = PGM_PAGE_GET_PAGEID(pPage);
        Assert(PGM_PAGE_GET_PAGEID(pPage) != NIL_GMM_PAGEID);
        VM_FF_SET(pVM, VM_FF_PGM_NEED_HANDY_PAGES);

        Log2(("PGM: Replaced shared page %#x at %RGp with %#x / %RHp\n", PGM_PAGE_GET_PAGEID(pPage),
              GCPhys, pVM->pgm.s.aHandyPages[iHandyPage].idPage, HCPhys));
        STAM_COUNTER_INC(&pVM->pgm.s.CTX_MID_Z(Stat,PageReplaceShared));
        pVM->pgm.s.cSharedPages--;
        AssertMsgFailed(("TODO: copy shared page content")); /** @todo err.. what about copying the page content? */
    }
    else
    {
        Log2(("PGM: Replaced zero page %RGp with %#x / %RHp\n", GCPhys, pVM->pgm.s.aHandyPages[iHandyPage].idPage, HCPhys));
        STAM_COUNTER_INC(&pVM->pgm.s.StatRZPageReplaceZero);
        pVM->pgm.s.cZeroPages--;
        Assert(pVM->pgm.s.aHandyPages[iHandyPage].idSharedPage == NIL_GMM_PAGEID);
    }

    /*
     * Do the PGMPAGE modifications.
     */
    pVM->pgm.s.cPrivatePages++;
    PGM_PAGE_SET_HCPHYS(pPage, HCPhys);
    PGM_PAGE_SET_PAGEID(pPage, pVM->pgm.s.aHandyPages[iHandyPage].idPage);
    PGM_PAGE_SET_STATE(pPage, PGM_PAGE_STATE_ALLOCATED);

    if (    fFlushTLBs
        &&  rc != VINF_PGM_GCPHYS_ALIASED)
        PGM_INVL_GUEST_TLBS();
    return rc;
}


/**
 * Deal with pages that are not writable, i.e. not in the ALLOCATED state.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VINF_PGM_SYNC_CR3 on success and a page pool flush is pending.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid page but has no physical backing.
 *
 * @param   pVM         The VM address.
 * @param   pPage       The physical page tracking structure.
 * @param   GCPhys      The address of the page.
 *
 * @remarks Called from within the PGM critical section.
 */
int pgmPhysPageMakeWritable(PVM pVM, PPGMPAGE pPage, RTGCPHYS GCPhys)
{
    switch (PGM_PAGE_GET_STATE(pPage))
    {
        case PGM_PAGE_STATE_WRITE_MONITORED:
            PGM_PAGE_SET_WRITTEN_TO(pPage);
            PGM_PAGE_SET_STATE(pPage, PGM_PAGE_STATE_ALLOCATED);
            /* fall thru */
        default: /* to shut up GCC */
        case PGM_PAGE_STATE_ALLOCATED:
            return VINF_SUCCESS;

        /*
         * Zero pages can be dummy pages for MMIO or reserved memory,
         * so we need to check the flags before joining cause with
         * shared page replacement.
         */
        case PGM_PAGE_STATE_ZERO:
            if (PGM_PAGE_IS_MMIO(pPage))
                return VERR_PGM_PHYS_PAGE_RESERVED;
            /* fall thru */
        case PGM_PAGE_STATE_SHARED:
            return pgmPhysAllocPage(pVM, pPage, GCPhys);
    }
}


/**
 * Wrapper for pgmPhysPageMakeWritable which enters the critsect.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VINF_PGM_SYNC_CR3 on success and a page pool flush is pending.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid page but has no physical backing.
 *
 * @param   pVM         The VM address.
 * @param   pPage       The physical page tracking structure.
 * @param   GCPhys      The address of the page.
 */
int pgmPhysPageMakeWritableUnlocked(PVM pVM, PPGMPAGE pPage, RTGCPHYS GCPhys)
{
    int rc = pgmLock(pVM);
    if (RT_SUCCESS(rc))
    {
        rc = pgmPhysPageMakeWritable(pVM, pPage, GCPhys);
        pgmUnlock(pVM);
    }
    return rc;
}


/**
 * Internal usage: Map the page specified by its GMM ID.
 *
 * This is similar to pgmPhysPageMap
 *
 * @returns VBox status code.
 *
 * @param   pVM         The VM handle.
 * @param   idPage      The Page ID.
 * @param   HCPhys      The physical address (for RC).
 * @param   ppv         Where to store the mapping address.
 *
 * @remarks Called from within the PGM critical section.
 */
int pgmPhysPageMapByPageID(PVM pVM, uint32_t idPage, RTHCPHYS HCPhys, void **ppv)
{
    /*
     * Validation.
     */
    Assert(PDMCritSectIsOwner(&pVM->pgm.s.CritSect));
    AssertReturn(HCPhys && !(HCPhys & PAGE_OFFSET_MASK), VERR_INVALID_PARAMETER);
    const uint32_t idChunk = idPage >> GMM_CHUNKID_SHIFT;
    AssertReturn(idChunk != NIL_GMM_CHUNKID, VERR_INVALID_PARAMETER);

#ifdef IN_RC
    /*
     * Map it by HCPhys.
     */
    return PGMDynMapHCPage(pVM, HCPhys, ppv);

#elif defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
    /*
     * Map it by HCPhys.
     */
    return pgmR0DynMapHCPageInlined(&pVM->pgm.s, HCPhys, ppv);

#else
    /*
     * Find/make Chunk TLB entry for the mapping chunk.
     */
    PPGMCHUNKR3MAP pMap;
    PPGMCHUNKR3MAPTLBE pTlbe = &pVM->pgm.s.ChunkR3Map.Tlb.aEntries[PGM_CHUNKR3MAPTLB_IDX(idChunk)];
    if (pTlbe->idChunk == idChunk)
    {
        STAM_COUNTER_INC(&pVM->pgm.s.CTX_MID_Z(Stat,ChunkR3MapTlbHits));
        pMap = pTlbe->pChunk;
    }
    else
    {
        STAM_COUNTER_INC(&pVM->pgm.s.CTX_MID_Z(Stat,ChunkR3MapTlbMisses));

        /*
         * Find the chunk, map it if necessary.
         */
        pMap = (PPGMCHUNKR3MAP)RTAvlU32Get(&pVM->pgm.s.ChunkR3Map.pTree, idChunk);
        if (!pMap)
        {
# ifdef IN_RING0
            int rc = VMMR0CallHost(pVM, VMMCALLHOST_PGM_MAP_CHUNK, idChunk);
            AssertRCReturn(rc, rc);
            pMap = (PPGMCHUNKR3MAP)RTAvlU32Get(&pVM->pgm.s.ChunkR3Map.pTree, idChunk);
            Assert(pMap);
# else
            int rc = pgmR3PhysChunkMap(pVM, idChunk, &pMap);
            if (RT_FAILURE(rc))
                return rc;
# endif
        }

        /*
         * Enter it into the Chunk TLB.
         */
        pTlbe->idChunk = idChunk;
        pTlbe->pChunk = pMap;
        pMap->iAge = 0;
    }

    *ppv = (uint8_t *)pMap->pv + ((idPage &GMM_PAGEID_IDX_MASK) << PAGE_SHIFT);
    return VINF_SUCCESS;
#endif
}


/**
 * Maps a page into the current virtual address space so it can be accessed.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid page but has no physical backing.
 *
 * @param   pVM         The VM address.
 * @param   pPage       The physical page tracking structure.
 * @param   GCPhys      The address of the page.
 * @param   ppMap       Where to store the address of the mapping tracking structure.
 * @param   ppv         Where to store the mapping address of the page. The page
 *                      offset is masked off!
 *
 * @remarks Called from within the PGM critical section.
 */
int pgmPhysPageMap(PVM pVM, PPGMPAGE pPage, RTGCPHYS GCPhys, PPPGMPAGEMAP ppMap, void **ppv)
{
    Assert(PDMCritSectIsOwner(&pVM->pgm.s.CritSect));

#if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
    /*
     * Just some sketchy GC/R0-darwin code.
     */
    *ppMap = NULL;
    RTHCPHYS HCPhys = PGM_PAGE_GET_HCPHYS(pPage);
    Assert(HCPhys != pVM->pgm.s.HCPhysZeroPg);
# ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
    pgmR0DynMapHCPageInlined(&pVM->pgm.s, HCPhys, ppv);
# else
    PGMDynMapHCPage(pVM, HCPhys, ppv);
# endif
    return VINF_SUCCESS;

#else /* IN_RING3 || IN_RING0 */


    /*
     * Special case: ZERO and MMIO2 pages.
     */
    const uint32_t idChunk = PGM_PAGE_GET_CHUNKID(pPage);
    if (idChunk == NIL_GMM_CHUNKID)
    {
        AssertMsgReturn(PGM_PAGE_GET_PAGEID(pPage) == NIL_GMM_PAGEID, ("pPage=%R[pgmpage]\n", pPage), VERR_INTERNAL_ERROR_2);
        if (PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_MMIO2)
        {
            /* Lookup the MMIO2 range and use pvR3 to calc the address. */
            PPGMRAMRANGE pRam = pgmPhysGetRange(&pVM->pgm.s, GCPhys);
            AssertMsgReturn(pRam || !pRam->pvR3, ("pRam=%p pPage=%R[pgmpage]\n", pRam, pPage), VERR_INTERNAL_ERROR_2);
            *ppv = (void *)((uintptr_t)pRam->pvR3 + (GCPhys - pRam->GCPhys));
        }
        else if (PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_MMIO2_ALIAS_MMIO)
        {
            /** @todo deal with aliased MMIO2 pages somehow...
             * One solution would be to seed MMIO2 pages to GMM and get unique Page IDs for
             * them, that would also avoid this mess. It would actually be kind of
             * elegant... */
            AssertLogRelMsgFailedReturn(("%RGp\n", GCPhys), VERR_INTERNAL_ERROR_3);
        }
        else
        {
            /** @todo handle MMIO2 */
            AssertMsgReturn(PGM_PAGE_IS_ZERO(pPage), ("pPage=%R[pgmpage]\n", pPage), VERR_INTERNAL_ERROR_2);
            AssertMsgReturn(PGM_PAGE_GET_HCPHYS(pPage) == pVM->pgm.s.HCPhysZeroPg,
                            ("pPage=%R[pgmpage]\n", pPage),
                            VERR_INTERNAL_ERROR_2);
            *ppv = pVM->pgm.s.CTXALLSUFF(pvZeroPg);
        }
        *ppMap = NULL;
        return VINF_SUCCESS;
    }

    /*
     * Find/make Chunk TLB entry for the mapping chunk.
     */
    PPGMCHUNKR3MAP pMap;
    PPGMCHUNKR3MAPTLBE pTlbe = &pVM->pgm.s.ChunkR3Map.Tlb.aEntries[PGM_CHUNKR3MAPTLB_IDX(idChunk)];
    if (pTlbe->idChunk == idChunk)
    {
        STAM_COUNTER_INC(&pVM->pgm.s.CTX_MID_Z(Stat,ChunkR3MapTlbHits));
        pMap = pTlbe->pChunk;
    }
    else
    {
        STAM_COUNTER_INC(&pVM->pgm.s.CTX_MID_Z(Stat,ChunkR3MapTlbMisses));

        /*
         * Find the chunk, map it if necessary.
         */
        pMap = (PPGMCHUNKR3MAP)RTAvlU32Get(&pVM->pgm.s.ChunkR3Map.pTree, idChunk);
        if (!pMap)
        {
#ifdef IN_RING0
            int rc = VMMR0CallHost(pVM, VMMCALLHOST_PGM_MAP_CHUNK, idChunk);
            AssertRCReturn(rc, rc);
            pMap = (PPGMCHUNKR3MAP)RTAvlU32Get(&pVM->pgm.s.ChunkR3Map.pTree, idChunk);
            Assert(pMap);
#else
            int rc = pgmR3PhysChunkMap(pVM, idChunk, &pMap);
            if (RT_FAILURE(rc))
                return rc;
#endif
        }

        /*
         * Enter it into the Chunk TLB.
         */
        pTlbe->idChunk = idChunk;
        pTlbe->pChunk = pMap;
        pMap->iAge = 0;
    }

    *ppv = (uint8_t *)pMap->pv + (PGM_PAGE_GET_PAGE_IN_CHUNK(pPage) << PAGE_SHIFT);
    *ppMap = pMap;
    return VINF_SUCCESS;
#endif /* IN_RING3 */
}


#if !defined(IN_RC) && !defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
/**
 * Load a guest page into the ring-3 physical TLB.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid physical address.
 * @param   pPGM        The PGM instance pointer.
 * @param   GCPhys      The guest physical address in question.
 */
int pgmPhysPageLoadIntoTlb(PPGM pPGM, RTGCPHYS GCPhys)
{
    STAM_COUNTER_INC(&pPGM->CTX_MID_Z(Stat,PageMapTlbMisses));

    /*
     * Find the ram range.
     * 99.8% of requests are expected to be in the first range.
     */
    PPGMRAMRANGE pRam = pPGM->CTX_SUFF(pRamRanges);
    RTGCPHYS off = GCPhys - pRam->GCPhys;
    if (RT_UNLIKELY(off >= pRam->cb))
    {
        do
        {
            pRam = pRam->CTX_SUFF(pNext);
            if (!pRam)
                return VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS;
            off = GCPhys - pRam->GCPhys;
        } while (off >= pRam->cb);
    }

    /*
     * Map the page.
     * Make a special case for the zero page as it is kind of special.
     */
    PPGMPAGE pPage = &pRam->aPages[off >> PAGE_SHIFT];
    PPGMPAGEMAPTLBE pTlbe = &pPGM->CTXSUFF(PhysTlb).aEntries[PGM_PAGEMAPTLB_IDX(GCPhys)];
    if (!PGM_PAGE_IS_ZERO(pPage))
    {
        void *pv;
        PPGMPAGEMAP pMap;
        int rc = pgmPhysPageMap(PGM2VM(pPGM), pPage, GCPhys, &pMap, &pv);
        if (RT_FAILURE(rc))
            return rc;
        pTlbe->pMap = pMap;
        pTlbe->pv = pv;
    }
    else
    {
        Assert(PGM_PAGE_GET_HCPHYS(pPage) == pPGM->HCPhysZeroPg);
        pTlbe->pMap = NULL;
        pTlbe->pv = pPGM->CTXALLSUFF(pvZeroPg);
    }
    pTlbe->pPage = pPage;
    return VINF_SUCCESS;
}


/**
 * Load a guest page into the ring-3 physical TLB.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid physical address.
 *
 * @param   pPGM        The PGM instance pointer.
 * @param   pPage       Pointer to the PGMPAGE structure corresponding to
 *                      GCPhys.
 * @param   GCPhys      The guest physical address in question.
 */
int pgmPhysPageLoadIntoTlbWithPage(PPGM pPGM, PPGMPAGE pPage, RTGCPHYS GCPhys)
{
    STAM_COUNTER_INC(&pPGM->CTX_MID_Z(Stat,PageMapTlbMisses));

    /*
     * Map the page.
     * Make a special case for the zero page as it is kind of special.
     */
    PPGMPAGEMAPTLBE pTlbe = &pPGM->CTXSUFF(PhysTlb).aEntries[PGM_PAGEMAPTLB_IDX(GCPhys)];
    if (!PGM_PAGE_IS_ZERO(pPage))
    {
        void *pv;
        PPGMPAGEMAP pMap;
        int rc = pgmPhysPageMap(PGM2VM(pPGM), pPage, GCPhys, &pMap, &pv);
        if (RT_FAILURE(rc))
            return rc;
        pTlbe->pMap = pMap;
        pTlbe->pv = pv;
    }
    else
    {
        Assert(PGM_PAGE_GET_HCPHYS(pPage) == pPGM->HCPhysZeroPg);
        pTlbe->pMap = NULL;
        pTlbe->pv = pPGM->CTXALLSUFF(pvZeroPg);
    }
    pTlbe->pPage = pPage;
    return VINF_SUCCESS;
}
#endif /* !IN_RC && !VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0 */


/**
 * Internal version of PGMPhysGCPhys2CCPtr that expects the caller to
 * own the PGM lock and therefore not need to lock the mapped page.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid page but has no physical backing.
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid physical address.
 *
 * @param   pVM         The VM handle.
 * @param   GCPhys      The guest physical address of the page that should be mapped.
 * @param   pPage       Pointer to the PGMPAGE structure for the page.
 * @param   ppv         Where to store the address corresponding to GCPhys.
 *
 * @internal
 */
int pgmPhysGCPhys2CCPtrInternal(PVM pVM, PPGMPAGE pPage, RTGCPHYS GCPhys, void **ppv)
{
    int rc;
    AssertReturn(pPage, VERR_INTERNAL_ERROR);
    Assert(PDMCritSectIsOwner(&pVM->pgm.s.CritSect) || VM_IS_EMT(pVM));

    /*
     * Make sure the page is writable.
     */
    if (RT_UNLIKELY(PGM_PAGE_GET_STATE(pPage) != PGM_PAGE_STATE_ALLOCATED))
    {
        rc = pgmPhysPageMakeWritable(pVM, pPage, GCPhys);
        if (RT_FAILURE(rc))
            return rc;
        AssertMsg(rc == VINF_SUCCESS || rc == VINF_PGM_SYNC_CR3 /* not returned */, ("%Rrc\n", rc));
    }
    Assert(PGM_PAGE_GET_HCPHYS(pPage) != 0);

    /*
     * Get the mapping address.
     */
#if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
    *ppv = pgmDynMapHCPageOff(&pVM->pgm.s, PGM_PAGE_GET_HCPHYS(pPage) | (GCPhys & PAGE_OFFSET_MASK));
#else
    PPGMPAGEMAPTLBE pTlbe;
    rc = pgmPhysPageQueryTlbeWithPage(&pVM->pgm.s, pPage, GCPhys, &pTlbe);
    if (RT_FAILURE(rc))
        return rc;
    *ppv = (void *)((uintptr_t)pTlbe->pv | (GCPhys & PAGE_OFFSET_MASK));
#endif
    return VINF_SUCCESS;
}


/**
 * Internal version of PGMPhysGCPhys2CCPtrReadOnly that expects the caller to
 * own the PGM lock and therefore not need to lock the mapped page.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid page but has no physical backing.
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid physical address.
 *
 * @param   pVM         The VM handle.
 * @param   GCPhys      The guest physical address of the page that should be mapped.
 * @param   pPage       Pointer to the PGMPAGE structure for the page.
 * @param   ppv         Where to store the address corresponding to GCPhys.
 *
 * @internal
 */
int pgmPhysGCPhys2CCPtrInternalReadOnly(PVM pVM, PPGMPAGE pPage, RTGCPHYS GCPhys, const void **ppv)
{
    AssertReturn(pPage, VERR_INTERNAL_ERROR);
    Assert(PDMCritSectIsOwner(&pVM->pgm.s.CritSect) || VM_IS_EMT(pVM));
    Assert(PGM_PAGE_GET_HCPHYS(pPage) != 0);

    /*
     * Get the mapping address.
     */
#if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
    *ppv = pgmDynMapHCPageOff(&pVM->pgm.s, PGM_PAGE_GET_HCPHYS(pPage) | (GCPhys & PAGE_OFFSET_MASK)); /** @todo add a read only flag? */
#else
    PPGMPAGEMAPTLBE pTlbe;
    int rc = pgmPhysPageQueryTlbeWithPage(&pVM->pgm.s, pPage, GCPhys, &pTlbe);
    if (RT_FAILURE(rc))
        return rc;
    *ppv = (void *)((uintptr_t)pTlbe->pv | (GCPhys & PAGE_OFFSET_MASK));
#endif
    return VINF_SUCCESS;
}


/**
 * Requests the mapping of a guest page into the current context.
 *
 * This API should only be used for very short term, as it will consume
 * scarse resources (R0 and GC) in the mapping cache. When you're done
 * with the page, call PGMPhysReleasePageMappingLock() ASAP to release it.
 *
 * This API will assume your intention is to write to the page, and will
 * therefore replace shared and zero pages. If you do not intend to modify
 * the page, use the PGMPhysGCPhys2CCPtrReadOnly() API.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid page but has no physical backing.
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid physical address.
 *
 * @param   pVM         The VM handle.
 * @param   GCPhys      The guest physical address of the page that should be mapped.
 * @param   ppv         Where to store the address corresponding to GCPhys.
 * @param   pLock       Where to store the lock information that PGMPhysReleasePageMappingLock needs.
 *
 * @remarks The caller is responsible for dealing with access handlers.
 * @todo    Add an informational return code for pages with access handlers?
 *
 * @remark  Avoid calling this API from within critical sections (other than the
 *          PGM one) because of the deadlock risk. External threads may need to
 *          delegate jobs to the EMTs.
 * @thread  Any thread.
 */
VMMDECL(int) PGMPhysGCPhys2CCPtr(PVM pVM, RTGCPHYS GCPhys, void **ppv, PPGMPAGEMAPLOCK pLock)
{
#ifdef VBOX_WITH_NEW_PHYS_CODE
# if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)

    /*
     * Find the page and make sure it's writable.
     */
    PPGMPAGE pPage;
    int rc = pgmPhysGetPageEx(&pVM->pgm.s, GCPhys, &pPage);
    if (RT_SUCCESS(rc))
    {
        if (RT_UNLIKELY(PGM_PAGE_GET_STATE(pPage) != PGM_PAGE_STATE_ALLOCATED))
            rc = pgmPhysPageMakeWritable(pVM, pPage, GCPhys);
        if (RT_SUCCESS(rc))
        {
            *ppv = pgmDynMapHCPageOff(&pVM->pgm.s, PGM_PAGE_GET_HCPHYS(pPage) | (GCPhys & PAGE_OFFSET_MASK)); /** @todo add a read only flag? */
#if 0
            pLock->pvMap = 0;
            pLock->pvPage = pPage;
#else
            pLock->u32Dummy = UINT32_MAX;
#endif
            AssertMsg(rc == VINF_SUCCESS || rc == VINF_PGM_SYNC_CR3 /* not returned */, ("%Rrc\n", rc));
            rc = VINF_SUCCESS;
        }
    }

# else  /* IN_RING3 || IN_RING0 */
    int rc = pgmLock(pVM);
    AssertRCReturn(rc, rc);

    /*
     * Query the Physical TLB entry for the page (may fail).
     */
    PPGMPAGEMAPTLBE pTlbe;
    rc = pgmPhysPageQueryTlbe(&pVM->pgm.s, GCPhys, &pTlbe);
    if (RT_SUCCESS(rc))
    {
        /*
         * If the page is shared, the zero page, or being write monitored
         * it must be converted to an page that's writable if possible.
         */
        PPGMPAGE pPage = pTlbe->pPage;
        if (RT_UNLIKELY(PGM_PAGE_GET_STATE(pPage) != PGM_PAGE_STATE_ALLOCATED))
        {
            rc = pgmPhysPageMakeWritable(pVM, pPage, GCPhys);
            if (RT_SUCCESS(rc))
            {
                AssertMsg(rc == VINF_SUCCESS || rc == VINF_PGM_SYNC_CR3 /* not returned */, ("%Rrc\n", rc));
                rc = pgmPhysPageQueryTlbeWithPage(&pVM->pgm.s, pPage, GCPhys, &pTlbe);
            }
        }
        if (RT_SUCCESS(rc))
        {
            /*
             * Now, just perform the locking and calculate the return address.
             */
            PPGMPAGEMAP pMap = pTlbe->pMap;
            if (pMap)
                pMap->cRefs++;
#if 0 /** @todo implement locking properly */
            if (RT_LIKELY(pPage->cLocks != PGM_PAGE_MAX_LOCKS))
                if (RT_UNLIKELY(++pPage->cLocks == PGM_PAGE_MAX_LOCKS))
                {
                    AssertMsgFailed(("%RGp is entering permanent locked state!\n", GCPhys));
                    if (pMap)
                        pMap->cRefs++; /* Extra ref to prevent it from going away. */
                }
#endif
            *ppv = (void *)((uintptr_t)pTlbe->pv | (GCPhys & PAGE_OFFSET_MASK));
            pLock->pvPage = pPage;
            pLock->pvMap = pMap;
        }
    }

    pgmUnlock(pVM);
#endif /* IN_RING3 || IN_RING0 */
    return rc;

#else
    /*
     * Temporary fallback code.
     */
# if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
/** @todo @bugref{3202}: check up this path. */
    return PGMDynMapGCPageOff(pVM, GCPhys, ppv);
# else
    return PGMPhysGCPhys2R3Ptr(pVM, GCPhys, 1, (PRTR3PTR)ppv);
# endif
#endif
}


/**
 * Requests the mapping of a guest page into the current context.
 *
 * This API should only be used for very short term, as it will consume
 * scarse resources (R0 and GC) in the mapping cache. When you're done
 * with the page, call PGMPhysReleasePageMappingLock() ASAP to release it.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid page but has no physical backing.
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid physical address.
 *
 * @param   pVM         The VM handle.
 * @param   GCPhys      The guest physical address of the page that should be mapped.
 * @param   ppv         Where to store the address corresponding to GCPhys.
 * @param   pLock       Where to store the lock information that PGMPhysReleasePageMappingLock needs.
 *
 * @remarks The caller is responsible for dealing with access handlers.
 * @todo    Add an informational return code for pages with access handlers?
 *
 * @remark  Avoid calling this API from within critical sections (other than
 *          the PGM one) because of the deadlock risk.
 * @thread  Any thread.
 */
VMMDECL(int) PGMPhysGCPhys2CCPtrReadOnly(PVM pVM, RTGCPHYS GCPhys, void const **ppv, PPGMPAGEMAPLOCK pLock)
{
#ifdef VBOX_WITH_NEW_PHYS_CODE
# if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)

    /*
     * Find the page and make sure it's readable.
     */
    PPGMPAGE pPage;
    int rc = pgmPhysGetPageEx(&pVM->pgm.s, GCPhys, &pPage);
    if (RT_SUCCESS(rc))
    {
        if (RT_UNLIKELY(PGM_PAGE_IS_MMIO(pPage)))
            rc = VERR_PGM_PHYS_PAGE_RESERVED;
        else
        {
            *ppv = pgmDynMapHCPageOff(&pVM->pgm.s, PGM_PAGE_GET_HCPHYS(pPage) | (GCPhys & PAGE_OFFSET_MASK)); /** @todo add a read only flag? */
#if 0
            pLock->pvMap = 0;
            pLock->pvPage = pPage;
#else
            pLock->u32Dummy = UINT32_MAX;
#endif
            AssertMsg(rc == VINF_SUCCESS || rc == VINF_PGM_SYNC_CR3 /* not returned */, ("%Rrc\n", rc));
            rc = VINF_SUCCESS;
        }
    }

# else  /* IN_RING3 || IN_RING0 */
    int rc = pgmLock(pVM);
    AssertRCReturn(rc, rc);

    /*
     * Query the Physical TLB entry for the page (may fail).
     */
    PPGMPAGEMAPTLBE pTlbe;
    rc = pgmPhysPageQueryTlbe(&pVM->pgm.s, GCPhys, &pTlbe);
    if (RT_SUCCESS(rc))
    {
        /* MMIO pages doesn't have any readable backing. */
        PPGMPAGE pPage = pTlbe->pPage;
        if (RT_UNLIKELY(PGM_PAGE_IS_MMIO(pPage)))
            rc = VERR_PGM_PHYS_PAGE_RESERVED;
        else
        {
            /*
             * Now, just perform the locking and calculate the return address.
             */
            PPGMPAGEMAP pMap = pTlbe->pMap;
            if (pMap)
                pMap->cRefs++;
#if 0 /** @todo implement locking properly */
            if (RT_LIKELY(pPage->cLocks != PGM_PAGE_MAX_LOCKS))
                if (RT_UNLIKELY(++pPage->cLocks == PGM_PAGE_MAX_LOCKS))
                {
                    AssertMsgFailed(("%RGp is entering permanent locked state!\n", GCPhys));
                    if (pMap)
                        pMap->cRefs++; /* Extra ref to prevent it from going away. */
                }
#endif
            *ppv = (void *)((uintptr_t)pTlbe->pv | (GCPhys & PAGE_OFFSET_MASK));
            pLock->pvPage = pPage;
            pLock->pvMap = pMap;
        }
    }

    pgmUnlock(pVM);
#endif /* IN_RING3 || IN_RING0 */
    return rc;

#else  /* !VBOX_WITH_NEW_PHYS_CODE */
    /*
     * Fallback code.
     */
    return PGMPhysGCPhys2CCPtr(pVM, GCPhys, (void **)ppv, pLock);
#endif /* !VBOX_WITH_NEW_PHYS_CODE */
}


/**
 * Requests the mapping of a guest page given by virtual address into the current context.
 *
 * This API should only be used for very short term, as it will consume
 * scarse resources (R0 and GC) in the mapping cache. When you're done
 * with the page, call PGMPhysReleasePageMappingLock() ASAP to release it.
 *
 * This API will assume your intention is to write to the page, and will
 * therefore replace shared and zero pages. If you do not intend to modify
 * the page, use the PGMPhysGCPtr2CCPtrReadOnly() API.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PAGE_TABLE_NOT_PRESENT if the page directory for the virtual address isn't present.
 * @retval  VERR_PAGE_NOT_PRESENT if the page at the virtual address isn't present.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid page but has no physical backing.
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid physical address.
 *
 * @param   pVM         The VM handle.
 * @param   GCPhys      The guest physical address of the page that should be mapped.
 * @param   ppv         Where to store the address corresponding to GCPhys.
 * @param   pLock       Where to store the lock information that PGMPhysReleasePageMappingLock needs.
 *
 * @remark  Avoid calling this API from within critical sections (other than
 *          the PGM one) because of the deadlock risk.
 * @thread  EMT
 */
VMMDECL(int) PGMPhysGCPtr2CCPtr(PVM pVM, RTGCPTR GCPtr, void **ppv, PPGMPAGEMAPLOCK pLock)
{
    VM_ASSERT_EMT(pVM);
    RTGCPHYS GCPhys;
    int rc = PGMPhysGCPtr2GCPhys(pVM, GCPtr, &GCPhys);
    if (RT_SUCCESS(rc))
        rc = PGMPhysGCPhys2CCPtr(pVM, GCPhys, ppv, pLock);
    return rc;
}


/**
 * Requests the mapping of a guest page given by virtual address into the current context.
 *
 * This API should only be used for very short term, as it will consume
 * scarse resources (R0 and GC) in the mapping cache. When you're done
 * with the page, call PGMPhysReleasePageMappingLock() ASAP to release it.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PAGE_TABLE_NOT_PRESENT if the page directory for the virtual address isn't present.
 * @retval  VERR_PAGE_NOT_PRESENT if the page at the virtual address isn't present.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid page but has no physical backing.
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid physical address.
 *
 * @param   pVM         The VM handle.
 * @param   GCPhys      The guest physical address of the page that should be mapped.
 * @param   ppv         Where to store the address corresponding to GCPhys.
 * @param   pLock       Where to store the lock information that PGMPhysReleasePageMappingLock needs.
 *
 * @remark  Avoid calling this API from within critical sections (other than
 *          the PGM one) because of the deadlock risk.
 * @thread  EMT
 */
VMMDECL(int) PGMPhysGCPtr2CCPtrReadOnly(PVM pVM, RTGCPTR GCPtr, void const **ppv, PPGMPAGEMAPLOCK pLock)
{
    VM_ASSERT_EMT(pVM);
    RTGCPHYS GCPhys;
    int rc = PGMPhysGCPtr2GCPhys(pVM, GCPtr, &GCPhys);
    if (RT_SUCCESS(rc))
        rc = PGMPhysGCPhys2CCPtrReadOnly(pVM, GCPhys, ppv, pLock);
    return rc;
}


/**
 * Release the mapping of a guest page.
 *
 * This is the counter part of PGMPhysGCPhys2CCPtr, PGMPhysGCPhys2CCPtrReadOnly
 * PGMPhysGCPtr2CCPtr and PGMPhysGCPtr2CCPtrReadOnly.
 *
 * @param   pVM         The VM handle.
 * @param   pLock       The lock structure initialized by the mapping function.
 */
VMMDECL(void) PGMPhysReleasePageMappingLock(PVM pVM, PPGMPAGEMAPLOCK pLock)
{
#ifdef VBOX_WITH_NEW_PHYS_CODE
#if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
    /* currently nothing to do here. */
    Assert(pLock->u32Dummy == UINT32_MAX);
    pLock->u32Dummy = 0;

#else   /* IN_RING3 */
    PPGMPAGEMAP pMap = (PPGMPAGEMAP)pLock->pvMap;
    if (!pMap)
    {
        /* The ZERO page and MMIO2 ends up here. */
        Assert(pLock->pvPage);
        pLock->pvPage = NULL;
    }
    else
    {
        pgmLock(pVM);

# if 0 /** @todo implement page locking */
        PPGMPAGE pPage = (PPGMPAGE)pLock->pvPage;
        Assert(pPage->cLocks >= 1);
        if (pPage->cLocks != PGM_PAGE_MAX_LOCKS)
            pPage->cLocks--;
# endif

        Assert(pMap->cRefs >= 1);
        pMap->cRefs--;
        pMap->iAge = 0;

        pgmUnlock(pVM);
    }
#endif /* IN_RING3 */
#else
    NOREF(pVM);
    NOREF(pLock);
#endif
}


/**
 * Converts a GC physical address to a HC ring-3 pointer.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_PGM_PHYS_PAGE_RESERVED it it's a valid GC physical
 *          page but has no physical backing.
 * @returns VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid
 *          GC physical address.
 * @returns VERR_PGM_GCPHYS_RANGE_CROSSES_BOUNDARY if the range crosses
 *          a dynamic ram chunk boundary
 *
 * @param   pVM         The VM handle.
 * @param   GCPhys      The GC physical address to convert.
 * @param   cbRange     Physical range
 * @param   pR3Ptr      Where to store the R3 pointer on success.
 *
 * @deprecated  Avoid when possible!
 */
VMMDECL(int) PGMPhysGCPhys2R3Ptr(PVM pVM, RTGCPHYS GCPhys, RTUINT cbRange, PRTR3PTR pR3Ptr)
{
#ifdef VBOX_WITH_NEW_PHYS_CODE
/** @todo this is kind of hacky and needs some more work. */
    VM_ASSERT_EMT(pVM); /* no longer safe for use outside the EMT thread! */

    Log(("PGMPhysGCPhys2R3Ptr(,%RGp,%#x,): dont use this API!\n", GCPhys, cbRange)); /** @todo eliminate this API! */
# if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
    AssertFailedReturn(VERR_NOT_IMPLEMENTED);
# else
    pgmLock(pVM);

    PPGMRAMRANGE pRam;
    PPGMPAGE pPage;
    int rc = pgmPhysGetPageAndRangeEx(&pVM->pgm.s, GCPhys, &pPage, &pRam);
    if (RT_SUCCESS(rc))
        rc = pgmPhysGCPhys2CCPtrInternal(pVM, pPage, GCPhys, (void **)pR3Ptr);

    pgmUnlock(pVM);
    Assert(rc <= VINF_SUCCESS);
    return rc;
# endif

#else  /* !VBOX_WITH_NEW_PHYS_CODE */

    if ((GCPhys & PGM_DYNAMIC_CHUNK_BASE_MASK) != ((GCPhys+cbRange-1) & PGM_DYNAMIC_CHUNK_BASE_MASK))
    {
        AssertMsgFailed(("%RGp - %RGp crosses a chunk boundary!!\n", GCPhys, GCPhys+cbRange));
        LogRel(("PGMPhysGCPhys2HCPtr %RGp - %RGp crosses a chunk boundary!!\n", GCPhys, GCPhys+cbRange));
        return VERR_PGM_GCPHYS_RANGE_CROSSES_BOUNDARY;
    }

    PPGMRAMRANGE pRam;
    PPGMPAGE pPage;
    int rc = pgmPhysGetPageAndRangeEx(&pVM->pgm.s, GCPhys, &pPage, &pRam);
    if (RT_FAILURE(rc))
        return rc;

#ifndef PGM_IGNORE_RAM_FLAGS_RESERVED
    if (RT_UNLIKELY(PGM_PAGE_IS_RESERVED(pPage)))
        return VERR_PGM_PHYS_PAGE_RESERVED;
#endif

    RTGCPHYS off = GCPhys - pRam->GCPhys;
    if (RT_UNLIKELY(off + cbRange > pRam->cb))
    {
        AssertMsgFailed(("%RGp - %RGp crosses a chunk boundary!!\n", GCPhys, GCPhys + cbRange));
        return VERR_PGM_GCPHYS_RANGE_CROSSES_BOUNDARY;
    }

    if (pRam->fFlags & MM_RAM_FLAGS_DYNAMIC_ALLOC)
    {
        unsigned iChunk = (off >> PGM_DYNAMIC_CHUNK_SHIFT);
#if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0) /* ASSUMES this is a rare occurence */
        PRTR3UINTPTR paChunkR3Ptrs = (PRTR3UINTPTR)MMHyperR3ToCC(pVM, pRam->paChunkR3Ptrs);
        *pR3Ptr = (RTR3PTR)(paChunkR3Ptrs[iChunk] + (off & PGM_DYNAMIC_CHUNK_OFFSET_MASK));
#else
        *pR3Ptr = (RTR3PTR)(pRam->paChunkR3Ptrs[iChunk] + (off & PGM_DYNAMIC_CHUNK_OFFSET_MASK));
#endif
    }
    else if (RT_LIKELY(pRam->pvR3))
        *pR3Ptr = (RTR3PTR)((RTR3UINTPTR)pRam->pvR3 + off);
    else
        return VERR_PGM_PHYS_PAGE_RESERVED;
    return VINF_SUCCESS;
#endif /* !VBOX_WITH_NEW_PHYS_CODE */
}


#ifdef VBOX_STRICT
/**
 * PGMPhysGCPhys2R3Ptr convenience for use with assertions.
 *
 * @returns The R3Ptr, NIL_RTR3PTR on failure.
 * @param   pVM         The VM handle.
 * @param   GCPhys      The GC Physical addresss.
 * @param   cbRange     Physical range.
 *
 * @deprecated  Avoid when possible.
 */
VMMDECL(RTR3PTR) PGMPhysGCPhys2R3PtrAssert(PVM pVM, RTGCPHYS GCPhys, RTUINT cbRange)
{
    RTR3PTR R3Ptr;
    int rc = PGMPhysGCPhys2R3Ptr(pVM, GCPhys, cbRange, &R3Ptr);
    if (RT_SUCCESS(rc))
        return R3Ptr;
    return NIL_RTR3PTR;
}
#endif /* VBOX_STRICT */


/**
 * Converts a guest pointer to a GC physical address.
 *
 * This uses the current CR3/CR0/CR4 of the guest.
 *
 * @returns VBox status code.
 * @param   pVM         The VM Handle
 * @param   GCPtr       The guest pointer to convert.
 * @param   pGCPhys     Where to store the GC physical address.
 */
VMMDECL(int) PGMPhysGCPtr2GCPhys(PVM pVM, RTGCPTR GCPtr, PRTGCPHYS pGCPhys)
{
    int rc = PGM_GST_PFN(GetPage,pVM)(pVM, (RTGCUINTPTR)GCPtr, NULL, pGCPhys);
    if (pGCPhys && RT_SUCCESS(rc))
        *pGCPhys |= (RTGCUINTPTR)GCPtr & PAGE_OFFSET_MASK;
    return rc;
}


/**
 * Converts a guest pointer to a HC physical address.
 *
 * This uses the current CR3/CR0/CR4 of the guest.
 *
 * @returns VBox status code.
 * @param   pVM         The VM Handle
 * @param   GCPtr       The guest pointer to convert.
 * @param   pHCPhys     Where to store the HC physical address.
 */
VMMDECL(int) PGMPhysGCPtr2HCPhys(PVM pVM, RTGCPTR GCPtr, PRTHCPHYS pHCPhys)
{
    RTGCPHYS GCPhys;
    int rc = PGM_GST_PFN(GetPage,pVM)(pVM, (RTGCUINTPTR)GCPtr, NULL, &GCPhys);
    if (RT_SUCCESS(rc))
        rc = PGMPhysGCPhys2HCPhys(pVM, GCPhys | ((RTGCUINTPTR)GCPtr & PAGE_OFFSET_MASK), pHCPhys);
    return rc;
}


/**
 * Converts a guest pointer to a R3 pointer.
 *
 * This uses the current CR3/CR0/CR4 of the guest.
 *
 * @returns VBox status code.
 * @param   pVM         The VM Handle
 * @param   GCPtr       The guest pointer to convert.
 * @param   pR3Ptr      Where to store the R3 virtual address.
 *
 * @deprecated Don't use this.
 */
VMMDECL(int) PGMPhysGCPtr2R3Ptr(PVM pVM, RTGCPTR GCPtr, PRTR3PTR pR3Ptr)
{
#ifdef VBOX_WITH_NEW_PHYS_CODE
    VM_ASSERT_EMT(pVM); /* no longer safe for use outside the EMT thread! */
#endif

    RTGCPHYS GCPhys;
    int rc = PGM_GST_PFN(GetPage,pVM)(pVM, (RTGCUINTPTR)GCPtr, NULL, &GCPhys);
    if (RT_SUCCESS(rc))
        rc = PGMPhysGCPhys2R3Ptr(pVM, GCPhys | ((RTGCUINTPTR)GCPtr & PAGE_OFFSET_MASK), 1 /* we always stay within one page */, pR3Ptr);
    return rc;
}



#undef LOG_GROUP
#define LOG_GROUP LOG_GROUP_PGM_PHYS_ACCESS


#ifdef IN_RING3
/**
 * Cache PGMPhys memory access
 *
 * @param   pVM             VM Handle.
 * @param   pCache          Cache structure pointer
 * @param   GCPhys          GC physical address
 * @param   pbHC            HC pointer corresponding to physical page
 *
 * @thread  EMT.
 */
static void pgmPhysCacheAdd(PVM pVM, PGMPHYSCACHE *pCache, RTGCPHYS GCPhys, uint8_t *pbR3)
{
    uint32_t iCacheIndex;

    Assert(VM_IS_EMT(pVM));

    GCPhys = PHYS_PAGE_ADDRESS(GCPhys);
    pbR3   = (uint8_t *)PAGE_ADDRESS(pbR3);

    iCacheIndex = ((GCPhys >> PAGE_SHIFT) & PGM_MAX_PHYSCACHE_ENTRIES_MASK);

    ASMBitSet(&pCache->aEntries, iCacheIndex);

    pCache->Entry[iCacheIndex].GCPhys = GCPhys;
    pCache->Entry[iCacheIndex].pbR3   = pbR3;
}
#endif /* IN_RING3 */

#ifdef VBOX_WITH_NEW_PHYS_CODE

/**
 * Deals with reading from a page with one or more ALL access handlers.
 *
 * @returns VBox status code. Can be ignored in ring-3.
 * @retval  VINF_SUCCESS.
 * @retval  VERR_PGM_PHYS_WR_HIT_HANDLER in R0 and GC, NEVER in R3.
 *
 * @param   pVM         The VM handle.
 * @param   pPage       The page descriptor.
 * @param   GCPhys      The physical address to start reading at.
 * @param   pvBuf       Where to put the bits we read.
 * @param   cb          How much to read - less or equal to a page.
 */
static int pgmPhysReadHandler(PVM pVM, PPGMPAGE pPage, RTGCPHYS GCPhys, void *pvBuf, size_t cb)
{
    /*
     * The most frequent access here is MMIO and shadowed ROM.
     * The current code ASSUMES all these access handlers covers full pages!
     */

    /*
     * Whatever we do we need the source page, map it first.
     */
    const void *pvSrc = NULL;
    int         rc    = pgmPhysGCPhys2CCPtrInternalReadOnly(pVM, pPage, GCPhys, &pvSrc);
    if (RT_FAILURE(rc))
    {
        AssertLogRelMsgFailed(("pgmPhysGCPhys2CCPtrInternalReadOnly failed on %RGp / %R[pgmpage] -> %Rrc\n",
                               GCPhys, pPage, rc));
        memset(pvBuf, 0xff, cb);
        return VINF_SUCCESS;
    }
    rc = VINF_PGM_HANDLER_DO_DEFAULT;

    /*
     * Deal with any physical handlers.
     */
    PPGMPHYSHANDLER pPhys = NULL;
    if (PGM_PAGE_GET_HNDL_PHYS_STATE(pPage) == PGM_PAGE_HNDL_PHYS_STATE_ALL)
    {
#ifdef IN_RING3
        PPGMPHYSHANDLER pPhys = (PPGMPHYSHANDLER)RTAvlroGCPhysRangeGet(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysHandlers, GCPhys);
        AssertReleaseMsg(pPhys, ("GCPhys=%RGp cb=%#x\n", GCPhys, cb));
        Assert(GCPhys >= pPhys->Core.Key && GCPhys <= pPhys->Core.KeyLast);
        Assert((pPhys->Core.Key     & PAGE_OFFSET_MASK) == 0);
        Assert((pPhys->Core.KeyLast & PAGE_OFFSET_MASK) == PAGE_OFFSET_MASK);
        Assert(pPhys->CTX_SUFF(pfnHandler));

        Log5(("pgmPhysReadHandler: GCPhys=%RGp cb=%#x pPage=%R[pgmpage] phys %s\n", GCPhys, cb, pPage, R3STRING(pPhys->pszDesc) ));
        STAM_PROFILE_START(&pPhys->Stat, h);
        rc = pPhys->CTX_SUFF(pfnHandler)(pVM, GCPhys, (void *)pvSrc, pvBuf, cb, PGMACCESSTYPE_READ, pPhys->CTX_SUFF(pvUser));
        STAM_PROFILE_STOP(&pPhys->Stat, h);
        AssertLogRelMsg(rc == VINF_SUCCESS || rc == VINF_PGM_HANDLER_DO_DEFAULT, ("rc=%Rrc GCPhys=%RGp\n", rc, GCPhys));
#else
        /* In R0 and RC the callbacks cannot handle this context, so we'll fail. */
        //AssertReleaseMsgFailed(("Wrong API! GCPhys=%RGp cb=%#x\n", GCPhys, cb));
        return VERR_PGM_PHYS_WR_HIT_HANDLER;
#endif
    }

    /*
     * Deal with any virtual handlers.
     */
    if (PGM_PAGE_GET_HNDL_VIRT_STATE(pPage) == PGM_PAGE_HNDL_VIRT_STATE_ALL)
    {
        unsigned        iPage;
        PPGMVIRTHANDLER pVirt;

        int rc2 = pgmHandlerVirtualFindByPhysAddr(pVM, GCPhys, &pVirt, &iPage);
        AssertReleaseMsg(RT_SUCCESS(rc2), ("GCPhys=%RGp cb=%#x rc2=%Rrc\n", GCPhys, cb, rc2));
        Assert((pVirt->Core.Key     & PAGE_OFFSET_MASK) == 0);
        Assert((pVirt->Core.KeyLast & PAGE_OFFSET_MASK) == PAGE_OFFSET_MASK);
        Assert(GCPhys >= pVirt->aPhysToVirt[iPage].Core.Key && GCPhys <= pVirt->aPhysToVirt[iPage].Core.KeyLast);

#ifdef IN_RING3
        if (pVirt->pfnHandlerR3)
        {
            if (!pPhys)
                Log5(("pgmPhysWriteHandler: GCPhys=%RGp cb=%#x pPage=%R[pgmpage] virt %s\n", GCPhys, cb, pPage, R3STRING(pVirt->pszDesc) ));
            else
                Log(("pgmPhysWriteHandler: GCPhys=%RGp cb=%#x pPage=%R[pgmpage] phys/virt %s/%s\n", GCPhys, cb, pPage, R3STRING(pVirt->pszDesc), R3STRING(pPhys->pszDesc) ));
            RTGCUINTPTR GCPtr = ((RTGCUINTPTR)pVirt->Core.Key & PAGE_BASE_GC_MASK)
                              + (iPage << PAGE_SHIFT)
                              + (GCPhys & PAGE_OFFSET_MASK);

            STAM_PROFILE_START(&pVirt->Stat, h);
            rc2 = pVirt->CTX_SUFF(pfnHandler)(pVM, GCPtr, (void *)pvSrc, pvBuf, cb, PGMACCESSTYPE_READ, /*pVirt->CTX_SUFF(pvUser)*/ NULL);
            STAM_PROFILE_STOP(&pVirt->Stat, h);
            if (rc2 == VINF_SUCCESS)
                rc = VINF_SUCCESS;
            AssertLogRelMsg(rc2 == VINF_SUCCESS || rc2 == VINF_PGM_HANDLER_DO_DEFAULT, ("rc=%Rrc GCPhys=%RGp pPage=%R[pgmpage] %s\n", rc2, GCPhys, pPage, pVirt->pszDesc));
        }
        else
            Log5(("pgmPhysWriteHandler: GCPhys=%RGp cb=%#x pPage=%R[pgmpage] virt %s [no handler]\n", GCPhys, cb, pPage, R3STRING(pVirt->pszDesc) ));
#else
        /* In R0 and RC the callbacks cannot handle this context, so we'll fail. */
        //AssertReleaseMsgFailed(("Wrong API! GCPhys=%RGp cb=%#x\n", GCPhys, cb));
        return VERR_PGM_PHYS_WR_HIT_HANDLER;
#endif
    }

    /*
     * Take the default action.
     */
    if (rc == VINF_PGM_HANDLER_DO_DEFAULT)
        memcpy(pvBuf, pvSrc, cb);
    return rc;
}


/**
 * Read physical memory.
 *
 * This API respects access handlers and MMIO. Use PGMPhysSimpleReadGCPhys() if you
 * want to ignore those.
 *
 * @returns VBox status code. Can be ignored in ring-3.
 * @retval  VINF_SUCCESS.
 * @retval  VERR_PGM_PHYS_WR_HIT_HANDLER in R0 and GC, NEVER in R3.
 *
 * @param   pVM             VM Handle.
 * @param   GCPhys          Physical address start reading from.
 * @param   pvBuf           Where to put the read bits.
 * @param   cbRead          How many bytes to read.
 */
VMMDECL(int) PGMPhysRead(PVM pVM, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead)
{
    AssertMsgReturn(cbRead > 0, ("don't even think about reading zero bytes!\n"), VINF_SUCCESS);
    LogFlow(("PGMPhysRead: %RGp %d\n", GCPhys, cbRead));

    pgmLock(pVM);

    /*
     * Copy loop on ram ranges.
     */
    PPGMRAMRANGE pRam = pVM->pgm.s.CTX_SUFF(pRamRanges);
    for (;;)
    {
        /* Find range. */
        while (pRam && GCPhys > pRam->GCPhysLast)
            pRam = pRam->CTX_SUFF(pNext);
        /* Inside range or not? */
        if (pRam && GCPhys >= pRam->GCPhys)
        {
            /*
             * Must work our way thru this page by page.
             */
            RTGCPHYS off = GCPhys - pRam->GCPhys;
            while (off < pRam->cb)
            {
                unsigned iPage = off >> PAGE_SHIFT;
                PPGMPAGE pPage = &pRam->aPages[iPage];
                size_t   cb    = PAGE_SIZE - (off & PAGE_OFFSET_MASK);
                if (cb > cbRead)
                    cb = cbRead;

                /*
                 * Any ALL access handlers?
                 */
                if (RT_UNLIKELY(PGM_PAGE_HAS_ACTIVE_ALL_HANDLERS(pPage)))
                {
                    int rc = pgmPhysReadHandler(pVM, pPage, pRam->GCPhys + off, pvBuf, cb);
                    if (RT_FAILURE(rc))
                        return rc;
                }
                else
                {
                    /*
                     * Get the pointer to the page.
                     */
                    const void *pvSrc;
                    int rc = pgmPhysGCPhys2CCPtrInternalReadOnly(pVM, pPage, pRam->GCPhys + off, &pvSrc);
                    if (RT_SUCCESS(rc))
                        memcpy(pvBuf, pvSrc, cb);
                    else
                    {
                        AssertLogRelMsgFailed(("pgmPhysGCPhys2CCPtrInternalReadOnly failed on %RGp / %R[pgmpage] -> %Rrc\n",
                                               pRam->GCPhys + off, pPage, rc));
                        memset(pvBuf, 0xff, cb);
                    }
                }

                /* next page */
                if (cb >= cbRead)
                {
                    pgmUnlock(pVM);
                    return VINF_SUCCESS;
                }
                cbRead -= cb;
                off    += cb;
                pvBuf   = (char *)pvBuf + cb;
            } /* walk pages in ram range. */

            GCPhys = pRam->GCPhysLast + 1;
        }
        else
        {
            LogFlow(("PGMPhysRead: Unassigned %RGp size=%u\n", GCPhys, cbRead));

            /*
             * Unassigned address space.
             */
            if (!pRam)
                break;
            size_t cb = pRam->GCPhys - GCPhys;
            if (cb >= cbRead)
            {
                memset(pvBuf, 0xff, cbRead);
                break;
            }
            memset(pvBuf, 0xff, cb);

            cbRead -= cb;
            pvBuf   = (char *)pvBuf + cb;
            GCPhys += cb;
        }
    } /* Ram range walk */

    pgmUnlock(pVM);
    return VINF_SUCCESS;
}

#else /* Old PGMPhysRead */

/**
 * Read physical memory.
 *
 * This API respects access handlers and MMIO. Use PGMPhysSimpleReadGCPhys() if you
 * want to ignore those.
 *
 * @param   pVM             VM Handle.
 * @param   GCPhys          Physical address start reading from.
 * @param   pvBuf           Where to put the read bits.
 * @param   cbRead          How many bytes to read.
 */
VMMDECL(void) PGMPhysRead(PVM pVM, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead)
{
#ifdef IN_RING3
    bool fGrabbedLock = false;
#endif

    AssertMsg(cbRead > 0, ("don't even think about reading zero bytes!\n"));
    if (cbRead == 0)
        return;

    LogFlow(("PGMPhysRead: %RGp %d\n", GCPhys, cbRead));

#ifdef IN_RING3
    if (!VM_IS_EMT(pVM))
    {
        pgmLock(pVM);
        fGrabbedLock = true;
    }
#endif

    /*
     * Copy loop on ram ranges.
     */
    PPGMRAMRANGE pRam = pVM->pgm.s.CTX_SUFF(pRamRanges);
    for (;;)
    {
        /* Find range. */
        while (pRam && GCPhys > pRam->GCPhysLast)
            pRam = pRam->CTX_SUFF(pNext);
        /* Inside range or not? */
        if (pRam && GCPhys >= pRam->GCPhys)
        {
            /*
             * Must work our way thru this page by page.
             */
            RTGCPHYS off = GCPhys - pRam->GCPhys;
            while (off < pRam->cb)
            {
                unsigned iPage = off >> PAGE_SHIFT;
                PPGMPAGE pPage = &pRam->aPages[iPage];
                size_t   cb;

                /* Physical chunk in dynamically allocated range not present? */
                if (RT_UNLIKELY(!PGM_PAGE_GET_HCPHYS(pPage)))
                {
                    /* Treat it as reserved; return zeros */
                    cb = PAGE_SIZE - (off & PAGE_OFFSET_MASK);
                    if (cb >= cbRead)
                    {
                        memset(pvBuf, 0, cbRead);
                        goto l_End;
                    }
                    memset(pvBuf, 0, cb);
                }
                /* temp hacks, will be reorganized. */
                /*
                 * Physical handler.
                 */
                else if (   RT_UNLIKELY(PGM_PAGE_GET_HNDL_PHYS_STATE(pPage) >= PGM_PAGE_HNDL_PHYS_STATE_ALL)
                         && !(pPage->HCPhys & MM_RAM_FLAGS_MMIO)) /// @todo PAGE FLAGS
                {
                    int rc = VINF_PGM_HANDLER_DO_DEFAULT;
                    cb = PAGE_SIZE - (off & PAGE_OFFSET_MASK);

#ifdef IN_RING3 /** @todo deal with this in GC and R0! */
                    /* find and call the handler */
                    PPGMPHYSHANDLER pNode = (PPGMPHYSHANDLER)RTAvlroGCPhysRangeGet(&pVM->pgm.s.pTreesR3->PhysHandlers, GCPhys);
                    if (pNode && pNode->pfnHandlerR3)
                    {
                        size_t cbRange = pNode->Core.KeyLast - GCPhys + 1;
                        if (cbRange < cb)
                            cb = cbRange;
                        if (cb > cbRead)
                            cb = cbRead;

                        void *pvSrc = PGMRAMRANGE_GETHCPTR(pRam, off)

                        /* Note! Dangerous assumption that HC handlers don't do anything that really requires an EMT lock! */
                        rc = pNode->pfnHandlerR3(pVM, GCPhys, pvSrc, pvBuf, cb, PGMACCESSTYPE_READ, pNode->pvUserR3);
                    }
#endif /* IN_RING3 */
                    if (rc == VINF_PGM_HANDLER_DO_DEFAULT)
                    {
#if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
                        void *pvSrc = pgmDynMapHCPageOff(&pVM->pgm.s, PGM_PAGE_GET_HCPHYS(pPage) + (off & PAGE_OFFSET_MASK));
#else
                        void *pvSrc = PGMRAMRANGE_GETHCPTR(pRam, off)
#endif

                        if (cb >= cbRead)
                        {
                            memcpy(pvBuf, pvSrc, cbRead);
                            goto l_End;
                        }
                        memcpy(pvBuf, pvSrc, cb);
                    }
                    else if (cb >= cbRead)
                        goto l_End;
                }
                /*
                 * Virtual handlers.
                 */
                else if (   RT_UNLIKELY(PGM_PAGE_GET_HNDL_VIRT_STATE(pPage) >= PGM_PAGE_HNDL_VIRT_STATE_ALL)
                         && !(pPage->HCPhys & MM_RAM_FLAGS_MMIO)) /// @todo PAGE FLAGS
                {
                    int rc = VINF_PGM_HANDLER_DO_DEFAULT;
                    cb = PAGE_SIZE - (off & PAGE_OFFSET_MASK);
#ifdef IN_RING3 /** @todo deal with this in GC and R0! */
                    /* Search the whole tree for matching physical addresses (rather expensive!) */
                    PPGMVIRTHANDLER pNode;
                    unsigned iPage;
                    int rc2 = pgmHandlerVirtualFindByPhysAddr(pVM, GCPhys, &pNode, &iPage);
                    if (RT_SUCCESS(rc2) && pNode->pfnHandlerR3)
                    {
                        size_t cbRange = pNode->Core.KeyLast - GCPhys + 1;
                        if (cbRange < cb)
                            cb = cbRange;
                        if (cb > cbRead)
                            cb = cbRead;
                        RTGCUINTPTR GCPtr = ((RTGCUINTPTR)pNode->Core.Key & PAGE_BASE_GC_MASK)
                                          + (iPage << PAGE_SHIFT) + (off & PAGE_OFFSET_MASK);

                        void *pvSrc = PGMRAMRANGE_GETHCPTR(pRam, off)

                        /* Note! Dangerous assumption that HC handlers don't do anything that really requires an EMT lock! */
                        rc = pNode->pfnHandlerR3(pVM, (RTGCPTR)GCPtr, pvSrc, pvBuf, cb, PGMACCESSTYPE_READ, 0);
                    }
#endif /* IN_RING3 */
                    if (rc == VINF_PGM_HANDLER_DO_DEFAULT)
                    {
#if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
                        void *pvSrc = pgmDynMapHCPageOff(&pVM->pgm.s, PGM_PAGE_GET_HCPHYS(pPage) + (off & PAGE_OFFSET_MASK));
#else
                        void *pvSrc = PGMRAMRANGE_GETHCPTR(pRam, off)
#endif
                        if (cb >= cbRead)
                        {
                            memcpy(pvBuf, pvSrc, cbRead);
                            goto l_End;
                        }
                        memcpy(pvBuf, pvSrc, cb);
                    }
                    else if (cb >= cbRead)
                        goto l_End;
                }
                else
                {
                    switch (pPage->HCPhys & (MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_MMIO | MM_RAM_FLAGS_ROM)) /** @todo PAGE FLAGS */
                    {
                        /*
                         * Normal memory or ROM.
                         */
                        case 0:
                        case MM_RAM_FLAGS_ROM:
                        case MM_RAM_FLAGS_ROM | MM_RAM_FLAGS_RESERVED:
                        //case MM_RAM_FLAGS_ROM | MM_RAM_FLAGS_MMIO2: /* = shadow */ - //MMIO2 isn't in the mask.
                        case MM_RAM_FLAGS_MMIO2: // MMIO2 isn't in the mask.
                        {
#if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
                            void *pvSrc = pgmDynMapHCPageOff(&pVM->pgm.s, PGM_PAGE_GET_HCPHYS(pPage) + (off & PAGE_OFFSET_MASK));
#else
                            void *pvSrc = PGMRAMRANGE_GETHCPTR(pRam, off)
#endif
                            cb = PAGE_SIZE - (off & PAGE_OFFSET_MASK);
                            if (cb >= cbRead)
                            {
#if defined(IN_RING3) && defined(PGM_PHYSMEMACCESS_CACHING)
                                if (cbRead <= 4 && !fGrabbedLock /* i.e. EMT */)
                                    pgmPhysCacheAdd(pVM, &pVM->pgm.s.pgmphysreadcache, GCPhys, (uint8_t*)pvSrc);
#endif /* IN_RING3 && PGM_PHYSMEMACCESS_CACHING */
                                memcpy(pvBuf, pvSrc, cbRead);
                                goto l_End;
                            }
                            memcpy(pvBuf, pvSrc, cb);
                            break;
                        }

                        /*
                         * All reserved, nothing there.
                         */
                        case MM_RAM_FLAGS_RESERVED:
                            cb = PAGE_SIZE - (off & PAGE_OFFSET_MASK);
                            if (cb >= cbRead)
                            {
                                memset(pvBuf, 0, cbRead);
                                goto l_End;
                            }
                            memset(pvBuf, 0, cb);
                            break;

                        /*
                         * The rest needs to be taken more carefully.
                         */
                        default:
#if 1                   /** @todo r=bird: Can you do this properly please. */
                            /** @todo Try MMIO; quick hack */
                            if (cbRead <= 8 && IOMMMIORead(pVM, GCPhys, (uint32_t *)pvBuf, cbRead) == VINF_SUCCESS)
                                goto l_End;
#endif

                            /** @todo fix me later. */
                            AssertReleaseMsgFailed(("Unknown read at %RGp size %u implement the complex physical reading case %RHp\n",
                                                    GCPhys, cbRead,
                                                    pPage->HCPhys & (MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_MMIO | MM_RAM_FLAGS_ROM))); /** @todo PAGE FLAGS */
                            cb = PAGE_SIZE - (off & PAGE_OFFSET_MASK);
                            break;
                    }
                }

                cbRead -= cb;
                off    += cb;
                pvBuf   = (char *)pvBuf + cb;
            }

            GCPhys = pRam->GCPhysLast + 1;
        }
        else
        {
            LogFlow(("PGMPhysRead: Unassigned %RGp size=%u\n", GCPhys, cbRead));

            /*
             * Unassigned address space.
             */
            size_t cb;
            if (    !pRam
                ||  (cb = pRam->GCPhys - GCPhys) >= cbRead)
            {
                memset(pvBuf, 0, cbRead);
                goto l_End;
            }

            memset(pvBuf, 0, cb); /** @todo this is wrong, unassigne == 0xff not 0x00! */
            cbRead -= cb;
            pvBuf   = (char *)pvBuf + cb;
            GCPhys += cb;
        }
    }
l_End:
#ifdef IN_RING3
    if (fGrabbedLock)
        pgmUnlock(pVM);
#endif
    return;
}

#endif /* Old PGMPhysRead */
#ifdef VBOX_WITH_NEW_PHYS_CODE

/**
 * Deals with writing to a page with one or more WRITE or ALL access handlers.
 *
 * @returns VBox status code. Can be ignored in ring-3.
 * @retval  VINF_SUCCESS.
 * @retval  VERR_PGM_PHYS_WR_HIT_HANDLER in R0 and GC, NEVER in R3.
 *
 * @param   pVM         The VM handle.
 * @param   pPage       The page descriptor.
 * @param   GCPhys      The physical address to start writing at.
 * @param   pvBuf       What to write.
 * @param   cbWrite     How much to write - less or equal to a page.
 */
static int pgmPhysWriteHandler(PVM pVM, PPGMPAGE pPage, RTGCPHYS GCPhys, void const *pvBuf, size_t cbWrite)
{
    void *pvDst = NULL;
    int rc;

    /*
     * Give priority to physical handlers (like #PF does).
     *
     * Hope for a lonely physical handler first that covers the whole
     * write area. This should be a pretty frequent case with MMIO and
     * the heavy usage of full page handlers in the page pool.
     */
    if (    !PGM_PAGE_HAS_ACTIVE_VIRTUAL_HANDLERS(pPage)
        ||  PGM_PAGE_IS_MMIO(pPage) /* screw virtual handlers on MMIO pages */)
    {
        PPGMPHYSHANDLER pCur = (PPGMPHYSHANDLER)RTAvlroGCPhysRangeGet(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysHandlers, GCPhys);
        if (pCur)
        {
            Assert(GCPhys >= pCur->Core.Key && GCPhys <= pCur->Core.KeyLast);
            Assert(pCur->CTX_SUFF(pfnHandler));

            size_t cbRange = pCur->Core.KeyLast - GCPhys + 1;
            if (cbRange > cbWrite)
                cbRange = cbWrite;

#ifndef IN_RING3
            /* In R0 and RC the callbacks cannot handle this context, so we'll fail. */
            NOREF(cbRange);
            //AssertReleaseMsgFailed(("Wrong API! GCPhys=%RGp cbRange=%#x\n", GCPhys, cbRange));
            return VERR_PGM_PHYS_WR_HIT_HANDLER;

#else  /* IN_RING3 */
            Log5(("pgmPhysWriteHandler: GCPhys=%RGp cbRange=%#x pPage=%R[pgmpage] phys %s\n", GCPhys, cbRange, pPage, R3STRING(pCur->pszDesc) ));
            if (!PGM_PAGE_IS_MMIO(pPage))
                rc = pgmPhysGCPhys2CCPtrInternal(pVM, pPage, GCPhys, &pvDst);
            else
                rc = VINF_SUCCESS;
            if (RT_SUCCESS(rc))
            {
                STAM_PROFILE_START(&pCur->Stat, h);
                rc = pCur->CTX_SUFF(pfnHandler)(pVM, GCPhys, pvDst, (void *)pvBuf, cbRange, PGMACCESSTYPE_WRITE, pCur->CTX_SUFF(pvUser));
                STAM_PROFILE_STOP(&pCur->Stat, h);
                if (rc == VINF_PGM_HANDLER_DO_DEFAULT)
                    memcpy(pvDst, pvBuf, cbRange);
                else
                    AssertLogRelMsg(rc == VINF_SUCCESS, ("rc=%Rrc GCPhys=%RGp pPage=%R[pgmpage] %s\n", rc, GCPhys, pPage, pCur->pszDesc));
            }
            else
                AssertLogRelMsgFailedReturn(("pgmPhysGCPhys2CCPtrInternal failed on %RGp / %R[pgmpage] -> %Rrc\n",
                                             GCPhys, pPage, rc), rc);
            if (RT_LIKELY(cbRange == cbWrite))
                return VINF_SUCCESS;

            /* more fun to be had below */
            cbWrite -= cbRange;
            GCPhys  += cbRange;
            pvBuf    = (uint8_t *)pvBuf + cbRange;
            pvDst    = (uint8_t *)pvDst + cbRange;
#endif /* IN_RING3 */
        }
        /* else: the handler is somewhere else in the page, deal with it below. */
        Assert(!PGM_PAGE_IS_MMIO(pPage)); /* MMIO handlers are all PAGE_SIZEed! */
    }
    /*
     * A virtual handler without any interfering physical handlers.
     * Hopefully it'll conver the whole write.
     */
    else if (!PGM_PAGE_HAS_ACTIVE_PHYSICAL_HANDLERS(pPage))
    {
        unsigned        iPage;
        PPGMVIRTHANDLER pCur;
        rc = pgmHandlerVirtualFindByPhysAddr(pVM, GCPhys, &pCur, &iPage);
        if (RT_SUCCESS(rc))
        {
            size_t cbRange = (PAGE_OFFSET_MASK & pCur->Core.KeyLast) - (PAGE_OFFSET_MASK & GCPhys) + 1;
            if (cbRange > cbWrite)
                cbRange = cbWrite;

#ifndef IN_RING3
            /* In R0 and RC the callbacks cannot handle this context, so we'll fail. */
            NOREF(cbRange);
            //AssertReleaseMsgFailed(("Wrong API! GCPhys=%RGp cbRange=%#x\n", GCPhys, cbRange));
            return VERR_PGM_PHYS_WR_HIT_HANDLER;

#else  /* IN_RING3 */

            Log5(("pgmPhysWriteHandler: GCPhys=%RGp cbRange=%#x pPage=%R[pgmpage] virt %s\n", GCPhys, cbRange, pPage, R3STRING(pCur->pszDesc) ));
            rc = pgmPhysGCPhys2CCPtrInternal(pVM, pPage, GCPhys, &pvDst);
            if (RT_SUCCESS(rc))
            {
                rc = VINF_PGM_HANDLER_DO_DEFAULT;
                if (pCur->pfnHandlerR3)
                {
                    RTGCUINTPTR GCPtr = ((RTGCUINTPTR)pCur->Core.Key & PAGE_BASE_GC_MASK)
                                      + (iPage << PAGE_SHIFT)
                                      + (GCPhys & PAGE_OFFSET_MASK);

                    STAM_PROFILE_START(&pCur->Stat, h);
                    rc = pCur->CTX_SUFF(pfnHandler)(pVM, GCPtr, pvDst, (void *)pvBuf, cbRange, PGMACCESSTYPE_WRITE, /*pCur->CTX_SUFF(pvUser)*/ NULL);
                    STAM_PROFILE_STOP(&pCur->Stat, h);
                }
                if (rc == VINF_PGM_HANDLER_DO_DEFAULT)
                    memcpy(pvDst, pvBuf, cbRange);
                else
                    AssertLogRelMsg(rc == VINF_SUCCESS, ("rc=%Rrc GCPhys=%RGp pPage=%R[pgmpage] %s\n", rc, GCPhys, pPage, pCur->pszDesc));
            }
            else
                AssertLogRelMsgFailedReturn(("pgmPhysGCPhys2CCPtrInternal failed on %RGp / %R[pgmpage] -> %Rrc\n",
                                             GCPhys, pPage, rc), rc);
            if (RT_LIKELY(cbRange == cbWrite))
                return VINF_SUCCESS;

            /* more fun to be had below */
            cbWrite -= cbRange;
            GCPhys  += cbRange;
            pvBuf    = (uint8_t *)pvBuf + cbRange;
            pvDst    = (uint8_t *)pvDst + cbRange;
#endif
        }
        /* else: the handler is somewhere else in the page, deal with it below. */
    }

    /*
     * Deal with all the odd ends.
     */

    /* We need a writable destination page. */
    if (!pvDst)
    {
        rc = pgmPhysGCPhys2CCPtrInternal(pVM, pPage, GCPhys, &pvDst);
        AssertLogRelMsgReturn(RT_SUCCESS(rc),
                              ("pgmPhysGCPhys2CCPtrInternal failed on %RGp / %R[pgmpage] -> %Rrc\n",
                               GCPhys, pPage, rc), rc);
    }

    /* The loop state (big + ugly). */
    unsigned        iVirtPage   = 0;
    PPGMVIRTHANDLER pVirt       = NULL;
    uint32_t        offVirt     = PAGE_SIZE;
    uint32_t        offVirtLast = PAGE_SIZE;
    bool            fMoreVirt   = PGM_PAGE_HAS_ACTIVE_VIRTUAL_HANDLERS(pPage);

    PPGMPHYSHANDLER pPhys       = NULL;
    uint32_t        offPhys     = PAGE_SIZE;
    uint32_t        offPhysLast = PAGE_SIZE;
    bool            fMorePhys   = PGM_PAGE_HAS_ACTIVE_PHYSICAL_HANDLERS(pPage);

    /* The loop. */
    for (;;)
    {
        /*
         * Find the closest handler at or above GCPhys.
         */
        if (fMoreVirt && !pVirt)
        {
            int rc = pgmHandlerVirtualFindByPhysAddr(pVM, GCPhys, &pVirt, &iVirtPage);
            if (RT_SUCCESS(rc))
            {
                offVirt = 0;
                offVirtLast = (pVirt->aPhysToVirt[iVirtPage].Core.KeyLast & PAGE_OFFSET_MASK) - (GCPhys & PAGE_OFFSET_MASK);
            }
            else
            {
                PPGMPHYS2VIRTHANDLER pVirtPhys;
                pVirtPhys = (PPGMPHYS2VIRTHANDLER)RTAvlroGCPhysGetBestFit(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysToVirtHandlers,
                                                                          GCPhys, true /* fAbove */);
                if (    pVirtPhys
                    &&  (pVirtPhys->Core.Key >> PAGE_SHIFT) == (GCPhys >> PAGE_SHIFT))
                {
                    /* ASSUME that pVirtPhys only covers one page. */
                    Assert((pVirtPhys->Core.Key >> PAGE_SHIFT) == (pVirtPhys->Core.KeyLast >> PAGE_SHIFT));
                    Assert(pVirtPhys->Core.Key > GCPhys);

                    pVirt       = (PPGMVIRTHANDLER)((uintptr_t)pVirtPhys + pVirtPhys->offVirtHandler);
                    iVirtPage   = pVirtPhys - &pVirt->aPhysToVirt[0]; Assert(iVirtPage == 0);
                    offVirt     = (pVirtPhys->Core.Key     & PAGE_OFFSET_MASK) - (GCPhys & PAGE_OFFSET_MASK);
                    offVirtLast = (pVirtPhys->Core.KeyLast & PAGE_OFFSET_MASK) - (GCPhys & PAGE_OFFSET_MASK);
                }
                else
                {
                    pVirt       = NULL;
                    fMoreVirt   = false;
                    offVirt     = offVirtLast = PAGE_SIZE;
                }
            }
        }

        if (fMorePhys && !pPhys)
        {
            pPhys = (PPGMPHYSHANDLER)RTAvlroGCPhysRangeGet(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysHandlers, GCPhys);
            if (pPhys)
            {
                offPhys = 0;
                offPhysLast = pPhys->Core.KeyLast - GCPhys; /* ASSUMES < 4GB handlers... */
            }
            else
            {
                pPhys = (PPGMPHYSHANDLER)RTAvlroGCPhysGetBestFit(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysHandlers,
                                                                 GCPhys, true /* fAbove */);
                if (    pPhys
                    &&  pPhys->Core.Key <= GCPhys + (cbWrite - 1))
                {
                    offPhys     = pPhys->Core.Key     - GCPhys;
                    offPhysLast = pPhys->Core.KeyLast - GCPhys; /* ASSUMES < 4GB handlers... */
                }
                else
                {
                    pPhys     = NULL;
                    fMorePhys = false;
                    offPhys   = offPhysLast = PAGE_SIZE;
                }
            }
        }

        /*
         * Handle access to space without handlers (that's easy).
         */
        rc = VINF_PGM_HANDLER_DO_DEFAULT;
        uint32_t cbRange = (uint32_t)cbWrite;
        if (offPhys && offVirt)
        {
            if (cbRange > offPhys)
                cbRange = offPhys;
            if (cbRange > offVirt)
                cbRange = offVirt;
            Log5(("pgmPhysWriteHandler: GCPhys=%RGp cbRange=%#x pPage=%R[pgmpage] miss\n", GCPhys, cbRange, pPage));
        }
        /*
         * Physical handler.
         */
        else if (!offPhys && offVirt)
        {
            if (cbRange > offPhysLast + 1)
                cbRange = offPhysLast + 1;
            if (cbRange > offVirt)
                cbRange = offVirt;
#ifdef IN_RING3
            Log5(("pgmPhysWriteHandler: GCPhys=%RGp cbRange=%#x pPage=%R[pgmpage] phys %s\n", GCPhys, cbRange, pPage, R3STRING(pPhys->pszDesc) ));
            STAM_PROFILE_START(&pPhys->Stat, h);
            rc = pPhys->CTX_SUFF(pfnHandler)(pVM, GCPhys, pvDst, (void *)pvBuf, cbRange, PGMACCESSTYPE_WRITE, pPhys->CTX_SUFF(pvUser));
            STAM_PROFILE_STOP(&pPhys->Stat, h);
            AssertLogRelMsg(rc == VINF_SUCCESS || rc == VINF_PGM_HANDLER_DO_DEFAULT, ("rc=%Rrc GCPhys=%RGp pPage=%R[pgmpage] %s\n", rc, GCPhys, pPage, pPhys->pszDesc));
            pPhys = NULL;
#else
            /* In R0 and RC the callbacks cannot handle this context, so we'll fail. */
            NOREF(cbRange);
            //AssertReleaseMsgFailed(("Wrong API! GCPhys=%RGp cbRange=%#x\n", GCPhys, cbRange));
            return VERR_PGM_PHYS_WR_HIT_HANDLER;
#endif
        }
        /*
         * Virtual handler.
         */
        else if (offPhys && !offVirt)
        {
            if (cbRange > offVirtLast + 1)
                cbRange = offVirtLast + 1;
            if (cbRange > offPhys)
                cbRange = offPhys;
#ifdef IN_RING3
            Log5(("pgmPhysWriteHandler: GCPhys=%RGp cbRange=%#x pPage=%R[pgmpage] phys %s\n", GCPhys, cbRange, pPage, R3STRING(pVirt->pszDesc) ));
            if (pVirt->pfnHandlerR3)
            {
                RTGCUINTPTR GCPtr = ((RTGCUINTPTR)pVirt->Core.Key & PAGE_BASE_GC_MASK)
                                  + (iVirtPage << PAGE_SHIFT)
                                  + (GCPhys & PAGE_OFFSET_MASK);
                STAM_PROFILE_START(&pVirt->Stat, h);
                rc = pVirt->CTX_SUFF(pfnHandler)(pVM, GCPtr, pvDst, (void *)pvBuf, cbRange, PGMACCESSTYPE_WRITE, /*pCur->CTX_SUFF(pvUser)*/ NULL);
                STAM_PROFILE_STOP(&pVirt->Stat, h);
                AssertLogRelMsg(rc == VINF_SUCCESS || rc == VINF_PGM_HANDLER_DO_DEFAULT, ("rc=%Rrc GCPhys=%RGp pPage=%R[pgmpage] %s\n", rc, GCPhys, pPage, pVirt->pszDesc));
            }
            pVirt = NULL;
#else
            /* In R0 and RC the callbacks cannot handle this context, so we'll fail. */
            NOREF(cbRange);
            //AssertReleaseMsgFailed(("Wrong API! GCPhys=%RGp cbRange=%#x\n", GCPhys, cbRange));
            return VERR_PGM_PHYS_WR_HIT_HANDLER;
#endif
        }
        /*
         * Both... give the physical one priority.
         */
        else
        {
            Assert(!offPhys && !offVirt);
            if (cbRange > offVirtLast + 1)
                cbRange = offVirtLast + 1;
            if (cbRange > offPhysLast + 1)
                cbRange = offPhysLast + 1;

#ifdef IN_RING3
            if (pVirt->pfnHandlerR3)
                Log(("pgmPhysWriteHandler: overlapping phys and virt handlers at %RGp %R[pgmpage]; cbRange=%#x\n", GCPhys, pPage, cbRange));
            Log5(("pgmPhysWriteHandler: GCPhys=%RGp cbRange=%#x pPage=%R[pgmpage] phys/virt %s/%s\n", GCPhys, cbRange, pPage, R3STRING(pPhys->pszDesc), R3STRING(pVirt->pszDesc) ));

            STAM_PROFILE_START(&pPhys->Stat, h);
            rc = pPhys->CTX_SUFF(pfnHandler)(pVM, GCPhys, pvDst, (void *)pvBuf, cbRange, PGMACCESSTYPE_WRITE, pPhys->CTX_SUFF(pvUser));
            STAM_PROFILE_STOP(&pPhys->Stat, h);
            AssertLogRelMsg(rc == VINF_SUCCESS || rc == VINF_PGM_HANDLER_DO_DEFAULT, ("rc=%Rrc GCPhys=%RGp pPage=%R[pgmpage] %s\n", rc, GCPhys, pPage, pPhys->pszDesc));
            if (pVirt->pfnHandlerR3)
            {

                RTGCUINTPTR GCPtr = ((RTGCUINTPTR)pVirt->Core.Key & PAGE_BASE_GC_MASK)
                                  + (iVirtPage << PAGE_SHIFT)
                                  + (GCPhys & PAGE_OFFSET_MASK);
                STAM_PROFILE_START(&pVirt->Stat, h);
                int rc2 = pVirt->CTX_SUFF(pfnHandler)(pVM, GCPtr, pvDst, (void *)pvBuf, cbRange, PGMACCESSTYPE_WRITE, /*pCur->CTX_SUFF(pvUser)*/ NULL);
                STAM_PROFILE_STOP(&pVirt->Stat, h);
                AssertLogRelMsg(rc2 != VINF_SUCCESS && rc2 != VINF_PGM_HANDLER_DO_DEFAULT, ("rc=%Rrc GCPhys=%RGp pPage=%R[pgmpage] %s\n", rc, GCPhys, pPage, pVirt->pszDesc));
                if (rc2 == VINF_SUCCESS && rc == VINF_PGM_HANDLER_DO_DEFAULT)
                    rc = VINF_SUCCESS;
            }
            pPhys = NULL;
            pVirt = NULL;
#else
            /* In R0 and RC the callbacks cannot handle this context, so we'll fail. */
            NOREF(cbRange);
            //AssertReleaseMsgFailed(("Wrong API! GCPhys=%RGp cbRange=%#x\n", GCPhys, cbRange));
            return VERR_PGM_PHYS_WR_HIT_HANDLER;
#endif
        }
        if (rc == VINF_PGM_HANDLER_DO_DEFAULT)
            memcpy(pvDst, pvBuf, cbRange);

        /*
         * Advance if we've got more stuff to do.
         */
        if (cbRange >= cbWrite)
            return VINF_SUCCESS;

        cbWrite         -= cbRange;
        GCPhys          += cbRange;
        pvBuf            = (uint8_t *)pvBuf + cbRange;
        pvDst            = (uint8_t *)pvDst + cbRange;

        offPhys         -= cbRange;
        offPhysLast     -= cbRange;
        offVirt         -= cbRange;
        offVirtLast     -= cbRange;
    }
}


/**
 * Write to physical memory.
 *
 * This API respects access handlers and MMIO. Use PGMPhysSimpleReadGCPhys() if you
 * want to ignore those.
 *
 * @returns VBox status code. Can be ignored in ring-3.
 * @retval  VINF_SUCCESS.
 * @retval  VERR_PGM_PHYS_WR_HIT_HANDLER in R0 and GC, NEVER in R3.
 *
 * @param   pVM             VM Handle.
 * @param   GCPhys          Physical address to write to.
 * @param   pvBuf           What to write.
 * @param   cbWrite         How many bytes to write.
 */
VMMDECL(int) PGMPhysWrite(PVM pVM, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite)
{
    AssertMsg(!pVM->pgm.s.fNoMorePhysWrites, ("Calling PGMPhysWrite after pgmR3Save()!\n"));
    AssertMsgReturn(cbWrite > 0, ("don't even think about writing zero bytes!\n"), VINF_SUCCESS);
    LogFlow(("PGMPhysWrite: %RGp %d\n", GCPhys, cbWrite));

    pgmLock(pVM);

    /*
     * Copy loop on ram ranges.
     */
    PPGMRAMRANGE    pRam = pVM->pgm.s.CTX_SUFF(pRamRanges);
    for (;;)
    {
        /* Find range. */
        while (pRam && GCPhys > pRam->GCPhysLast)
            pRam = pRam->CTX_SUFF(pNext);
        /* Inside range or not? */
        if (pRam && GCPhys >= pRam->GCPhys)
        {
            /*
             * Must work our way thru this page by page.
             */
            RTGCPTR off = GCPhys - pRam->GCPhys;
            while (off < pRam->cb)
            {
                RTGCPTR     iPage = off >> PAGE_SHIFT;
                PPGMPAGE    pPage = &pRam->aPages[iPage];
                size_t      cb    = PAGE_SIZE - (off & PAGE_OFFSET_MASK);
                if (cb > cbWrite)
                    cb = cbWrite;

                /*
                 * Any active WRITE or ALL access handlers?
                 */
                if (PGM_PAGE_HAS_ACTIVE_HANDLERS(pPage))
                {
                    int rc = pgmPhysWriteHandler(pVM, pPage, pRam->GCPhys + off, pvBuf, cb);
                    if (RT_FAILURE(rc))
                        return rc;
                }
                else
                {
                    /*
                     * Get the pointer to the page.
                     */
                    void *pvDst;
                    int rc = pgmPhysGCPhys2CCPtrInternal(pVM, pPage, pRam->GCPhys + off, &pvDst);
                    if (RT_SUCCESS(rc))
                        memcpy(pvDst, pvBuf, cb);
                    else
                        AssertLogRelMsgFailed(("pgmPhysGCPhys2CCPtrInternal failed on %RGp / %R[pgmpage] -> %Rrc\n",
                                               pRam->GCPhys + off, pPage, rc));
                }

                /* next page */
                if (cb >= cbWrite)
                {
                    pgmUnlock(pVM);
                    return VINF_SUCCESS;
                }

                cbWrite -= cb;
                off     += cb;
                pvBuf    = (const char *)pvBuf + cb;
            } /* walk pages in ram range */

            GCPhys = pRam->GCPhysLast + 1;
        }
        else
        {
            /*
             * Unassigned address space, skip it.
             */
            if (!pRam)
                break;
            size_t cb = pRam->GCPhys - GCPhys;
            if (cb >= cbWrite)
                break;
            cbWrite -= cb;
            pvBuf   = (const char *)pvBuf + cb;
            GCPhys += cb;
        }
    } /* Ram range walk */

    pgmUnlock(pVM);
    return VINF_SUCCESS;
}

#else /* Old PGMPhysWrite */

/**
 * Write to physical memory.
 *
 * This API respects access handlers and MMIO. Use PGMPhysSimpleReadGCPhys() if you
 * want to ignore those.
 *
 * @param   pVM             VM Handle.
 * @param   GCPhys          Physical address to write to.
 * @param   pvBuf           What to write.
 * @param   cbWrite         How many bytes to write.
 */
VMMDECL(void) PGMPhysWrite(PVM pVM, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite)
{
#ifdef IN_RING3
    bool fGrabbedLock = false;
#endif

    AssertMsg(!pVM->pgm.s.fNoMorePhysWrites, ("Calling PGMPhysWrite after pgmR3Save()!\n"));
    AssertMsg(cbWrite > 0, ("don't even think about writing zero bytes!\n"));
    if (cbWrite == 0)
        return;

    LogFlow(("PGMPhysWrite: %RGp %d\n", GCPhys, cbWrite));

#ifdef IN_RING3
    if (!VM_IS_EMT(pVM))
    {
        pgmLock(pVM);
        fGrabbedLock = true;
    }
#endif
    /*
     * Copy loop on ram ranges.
     */
    PPGMRAMRANGE    pRam = pVM->pgm.s.CTX_SUFF(pRamRanges);
    for (;;)
    {
        /* Find range. */
        while (pRam && GCPhys > pRam->GCPhysLast)
            pRam = pRam->CTX_SUFF(pNext);
        /* Inside range or not? */
        if (pRam && GCPhys >= pRam->GCPhys)
        {
            /*
             * Must work our way thru this page by page.
             */
            RTGCPTR off = GCPhys - pRam->GCPhys;
            while (off < pRam->cb)
            {
                RTGCPTR     iPage = off >> PAGE_SHIFT;
                PPGMPAGE    pPage = &pRam->aPages[iPage];

                /* Physical chunk in dynamically allocated range not present? */
                if (RT_UNLIKELY(!PGM_PAGE_GET_HCPHYS(pPage)))
                {
                    int         rc;
                    RTGCPHYS    GCPhysPage = pRam->GCPhys + off;
#ifdef IN_RING3
                    if (fGrabbedLock)
                    {
                        pgmUnlock(pVM);
                        rc = pgmr3PhysGrowRange(pVM, GCPhysPage);
                        if (rc == VINF_SUCCESS)
                            PGMPhysWrite(pVM, GCPhysPage, pvBuf, cbWrite); /* try again; can't assume pRam is still valid (paranoia) */
                        return;
                    }
                    rc = pgmr3PhysGrowRange(pVM, GCPhysPage);
#else
                    rc = CTXALLMID(VMM, CallHost)(pVM, VMMCALLHOST_PGM_RAM_GROW_RANGE, GCPhysPage);
#endif
                    if (rc != VINF_SUCCESS)
                        goto l_End;
                }

                size_t   cb;
                /* temporary hack, will reogranize is later. */
                /*
                 * Virtual handlers
                 */
                if (    PGM_PAGE_HAS_ACTIVE_VIRTUAL_HANDLERS(pPage)
                    && !(pPage->HCPhys & MM_RAM_FLAGS_MMIO)) /// @todo PAGE FLAGS
                {
                    if (PGM_PAGE_HAS_ACTIVE_PHYSICAL_HANDLERS(pPage))
                    {
                        /*
                         * Physical write handler + virtual write handler.
                         * Consider this a quick workaround for the CSAM + shadow caching problem.
                         *
                         * We hand it to the shadow caching first since it requires the unchanged
                         * data. CSAM will have to put up with it already being changed.
                         */
                        int rc = VINF_PGM_HANDLER_DO_DEFAULT;
                        cb = PAGE_SIZE - (off & PAGE_OFFSET_MASK);
#ifdef IN_RING3 /** @todo deal with this in GC and R0! */
                        /* 1. The physical handler */
                        PPGMPHYSHANDLER pPhysNode = (PPGMPHYSHANDLER)RTAvlroGCPhysRangeGet(&pVM->pgm.s.pTreesR3->PhysHandlers, GCPhys);
                        if (pPhysNode && pPhysNode->pfnHandlerR3)
                        {
                            size_t cbRange = pPhysNode->Core.KeyLast - GCPhys + 1;
                            if (cbRange < cb)
                                cb = cbRange;
                            if (cb > cbWrite)
                                cb = cbWrite;

                            void *pvDst = PGMRAMRANGE_GETHCPTR(pRam, off)

                            /* Note! Dangerous assumption that R3 handlers don't do anything that really requires an EMT lock! */
                            rc = pPhysNode->pfnHandlerR3(pVM, GCPhys, pvDst, (void *)pvBuf, cb, PGMACCESSTYPE_WRITE, pPhysNode->pvUserR3);
                        }

                        /* 2. The virtual handler (will see incorrect data) */
                        PPGMVIRTHANDLER pVirtNode;
                        unsigned iPage;
                        int rc2 = pgmHandlerVirtualFindByPhysAddr(pVM, GCPhys, &pVirtNode, &iPage);
                        if (RT_SUCCESS(rc2) && pVirtNode->pfnHandlerR3)
                        {
                            size_t cbRange = pVirtNode->Core.KeyLast - GCPhys + 1;
                            if (cbRange < cb)
                                cb = cbRange;
                            if (cb > cbWrite)
                                cb = cbWrite;
                            RTGCUINTPTR GCPtr = ((RTGCUINTPTR)pVirtNode->Core.Key & PAGE_BASE_GC_MASK)
                                              + (iPage << PAGE_SHIFT) + (off & PAGE_OFFSET_MASK);

                            void *pvDst = PGMRAMRANGE_GETHCPTR(pRam, off)

                            /* Note! Dangerous assumption that R3 handlers don't do anything that really requires an EMT lock! */
                            rc2 = pVirtNode->pfnHandlerR3(pVM, (RTGCPTR)GCPtr, pvDst, (void *)pvBuf, cb, PGMACCESSTYPE_WRITE, 0);
                            if (    (   rc2 != VINF_PGM_HANDLER_DO_DEFAULT
                                     && rc == VINF_PGM_HANDLER_DO_DEFAULT)
                                ||  (   RT_FAILURE(rc2)
                                     && RT_SUCCESS(rc)))
                                rc = rc2;
                        }
#endif /* IN_RING3 */
                        if (rc == VINF_PGM_HANDLER_DO_DEFAULT)
                        {
#if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
                            void *pvDst = pgmDynMapHCPageOff(&pVM->pgm.s, PGM_PAGE_GET_HCPHYS(pPage) + (off & PAGE_OFFSET_MASK));
#else
                            void *pvDst = PGMRAMRANGE_GETHCPTR(pRam, off)
#endif
                            if (cb >= cbWrite)
                            {
                                memcpy(pvDst, pvBuf, cbWrite);
                                goto l_End;
                            }
                            memcpy(pvDst, pvBuf, cb);
                        }
                        else if (cb >= cbWrite)
                            goto l_End;
                    }
                    else
                    {
                        int rc = VINF_PGM_HANDLER_DO_DEFAULT;
                        cb = PAGE_SIZE - (off & PAGE_OFFSET_MASK);
#ifdef IN_RING3
/** @todo deal with this in GC and R0! */
                        /* Search the whole tree for matching physical addresses (rather expensive!) */
                        PPGMVIRTHANDLER pNode;
                        unsigned iPage;
                        int rc2 = pgmHandlerVirtualFindByPhysAddr(pVM, GCPhys, &pNode, &iPage);
                        if (RT_SUCCESS(rc2) && pNode->pfnHandlerR3)
                        {
                            size_t cbRange = pNode->Core.KeyLast - GCPhys + 1;
                            if (cbRange < cb)
                                cb = cbRange;
                            if (cb > cbWrite)
                                cb = cbWrite;
                            RTGCUINTPTR GCPtr = ((RTGCUINTPTR)pNode->Core.Key & PAGE_BASE_GC_MASK)
                                              + (iPage << PAGE_SHIFT) + (off & PAGE_OFFSET_MASK);

                            void *pvDst = PGMRAMRANGE_GETHCPTR(pRam, off)

                            /* Note! Dangerous assumption that R3 handlers don't do anything that really requires an EMT lock! */
                            rc = pNode->pfnHandlerR3(pVM, (RTGCPTR)GCPtr, pvDst, (void *)pvBuf, cb, PGMACCESSTYPE_WRITE, 0);
                        }
#endif /* IN_RING3 */
                        if (rc == VINF_PGM_HANDLER_DO_DEFAULT)
                        {
#if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
                            void *pvDst = pgmDynMapHCPageOff(&pVM->pgm.s, PGM_PAGE_GET_HCPHYS(pPage) + (off & PAGE_OFFSET_MASK));
#else
                            void *pvDst = PGMRAMRANGE_GETHCPTR(pRam, off)
#endif
                            if (cb >= cbWrite)
                            {
                                memcpy(pvDst, pvBuf, cbWrite);
                                goto l_End;
                            }
                            memcpy(pvDst, pvBuf, cb);
                        }
                        else if (cb >= cbWrite)
                            goto l_End;
                    }
                }
                /*
                 * Physical handler.
                 */
                else if (   RT_UNLIKELY(PGM_PAGE_GET_HNDL_PHYS_STATE(pPage) >= PGM_PAGE_HNDL_PHYS_STATE_WRITE)
                         && !(pPage->HCPhys & MM_RAM_FLAGS_MMIO)) /// @todo PAGE FLAGS
                {
                    int rc = VINF_PGM_HANDLER_DO_DEFAULT;
                    cb = PAGE_SIZE - (off & PAGE_OFFSET_MASK);
#ifdef IN_RING3 /** @todo deal with this in GC and R0! */
                    /* find and call the handler */
                    PPGMPHYSHANDLER pNode = (PPGMPHYSHANDLER)RTAvlroGCPhysRangeGet(&pVM->pgm.s.pTreesR3->PhysHandlers, GCPhys);
                    if (pNode && pNode->pfnHandlerR3)
                    {
                        size_t cbRange = pNode->Core.KeyLast - GCPhys + 1;
                        if (cbRange < cb)
                            cb = cbRange;
                        if (cb > cbWrite)
                            cb = cbWrite;

                        void *pvDst = PGMRAMRANGE_GETHCPTR(pRam, off)

                        /** @todo Dangerous assumption that HC handlers don't do anything that really requires an EMT lock! */
                        rc = pNode->pfnHandlerR3(pVM, GCPhys, pvDst, (void *)pvBuf, cb, PGMACCESSTYPE_WRITE, pNode->pvUserR3);
                    }
#endif /* IN_RING3 */
                    if (rc == VINF_PGM_HANDLER_DO_DEFAULT)
                    {
#if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
                        void *pvDst = pgmDynMapHCPageOff(&pVM->pgm.s, PGM_PAGE_GET_HCPHYS(pPage) + (off & PAGE_OFFSET_MASK));
#else
                        void *pvDst = PGMRAMRANGE_GETHCPTR(pRam, off)
#endif
                        if (cb >= cbWrite)
                        {
                            memcpy(pvDst, pvBuf, cbWrite);
                            goto l_End;
                        }
                        memcpy(pvDst, pvBuf, cb);
                    }
                    else if (cb >= cbWrite)
                        goto l_End;
                }
                else
                {
                    /** @todo r=bird: missing MM_RAM_FLAGS_ROM here, we shall not allow anyone to overwrite the ROM! */
                    switch (pPage->HCPhys & (MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_MMIO | MM_RAM_FLAGS_MMIO2)) /** @todo PAGE FLAGS */
                    {
                        /*
                         * Normal memory, MMIO2 or writable shadow ROM.
                         */
                        case 0:
                        case MM_RAM_FLAGS_MMIO2:
                        case MM_RAM_FLAGS_ROM | MM_RAM_FLAGS_MMIO2: /* shadow rom */
                        {
#if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
                            void *pvDst = pgmDynMapHCPageOff(&pVM->pgm.s, PGM_PAGE_GET_HCPHYS(pPage) + (off & PAGE_OFFSET_MASK));
#else
                            void *pvDst = PGMRAMRANGE_GETHCPTR(pRam, off)
#endif
                            cb = PAGE_SIZE - (off & PAGE_OFFSET_MASK);
                            if (cb >= cbWrite)
                            {
#if defined(IN_RING3) && defined(PGM_PHYSMEMACCESS_CACHING)
                                if (cbWrite <= 4 && !fGrabbedLock /* i.e. EMT */)
                                    pgmPhysCacheAdd(pVM, &pVM->pgm.s.pgmphyswritecache, GCPhys, (uint8_t*)pvDst);
#endif /* IN_RING3 && PGM_PHYSMEMACCESS_CACHING */
                                memcpy(pvDst, pvBuf, cbWrite);
                                goto l_End;
                            }
                            memcpy(pvDst, pvBuf, cb);
                            break;
                        }

                        /*
                         * All reserved, nothing there.
                         */
                        case MM_RAM_FLAGS_RESERVED:
                        case MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_MMIO2:
                            cb = PAGE_SIZE - (off & PAGE_OFFSET_MASK);
                            if (cb >= cbWrite)
                                goto l_End;
                            break;


                        /*
                         * The rest needs to be taken more carefully.
                         */
                        default:
#if 1                   /** @todo r=bird: Can you do this properly please. */
                            /** @todo Try MMIO; quick hack */
                            if (cbWrite <= 8 && IOMMMIOWrite(pVM, GCPhys, *(uint32_t *)pvBuf, cbWrite) == VINF_SUCCESS)
                                goto l_End;
#endif

                            /** @todo fix me later. */
                            AssertReleaseMsgFailed(("Unknown write at %RGp size %u implement the complex physical writing case %RHp\n",
                                                    GCPhys, cbWrite,
                                                    (pPage->HCPhys & (MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_MMIO | MM_RAM_FLAGS_MMIO2)))); /** @todo PAGE FLAGS */
                            /* skip the write */
                            cb = cbWrite;
                            break;
                    }
                }

                cbWrite -= cb;
                off     += cb;
                pvBuf    = (const char *)pvBuf + cb;
            }

            GCPhys = pRam->GCPhysLast + 1;
        }
        else
        {
            /*
             * Unassigned address space.
             */
            size_t cb;
            if (    !pRam
                ||  (cb = pRam->GCPhys - GCPhys) >= cbWrite)
                goto l_End;

            cbWrite -= cb;
            pvBuf   = (const char *)pvBuf + cb;
            GCPhys += cb;
        }
    }
l_End:
#ifdef IN_RING3
    if (fGrabbedLock)
        pgmUnlock(pVM);
#endif
    return;
}

#endif /* Old PGMPhysWrite */


/**
 * Read from guest physical memory by GC physical address, bypassing
 * MMIO and access handlers.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   pvDst       The destination address.
 * @param   GCPhysSrc   The source address (GC physical address).
 * @param   cb          The number of bytes to read.
 */
VMMDECL(int) PGMPhysSimpleReadGCPhys(PVM pVM, void *pvDst, RTGCPHYS GCPhysSrc, size_t cb)
{
    /*
     * Treat the first page as a special case.
     */
    if (!cb)
        return VINF_SUCCESS;

    /* map the 1st page */
    void const *pvSrc;
    PGMPAGEMAPLOCK Lock;
    int rc = PGMPhysGCPhys2CCPtrReadOnly(pVM, GCPhysSrc, &pvSrc, &Lock);
    if (RT_FAILURE(rc))
        return rc;

    /* optimize for the case where access is completely within the first page. */
    size_t cbPage = PAGE_SIZE - (GCPhysSrc & PAGE_OFFSET_MASK);
    if (RT_LIKELY(cb <= cbPage))
    {
        memcpy(pvDst, pvSrc, cb);
        PGMPhysReleasePageMappingLock(pVM, &Lock);
        return VINF_SUCCESS;
    }

    /* copy to the end of the page. */
    memcpy(pvDst, pvSrc, cbPage);
    PGMPhysReleasePageMappingLock(pVM, &Lock);
    GCPhysSrc += cbPage;
    pvDst = (uint8_t *)pvDst + cbPage;
    cb -= cbPage;

    /*
     * Page by page.
     */
    for (;;)
    {
        /* map the page */
        rc = PGMPhysGCPhys2CCPtrReadOnly(pVM, GCPhysSrc, &pvSrc, &Lock);
        if (RT_FAILURE(rc))
            return rc;

        /* last page? */
        if (cb <= PAGE_SIZE)
        {
            memcpy(pvDst, pvSrc, cb);
            PGMPhysReleasePageMappingLock(pVM, &Lock);
            return VINF_SUCCESS;
        }

        /* copy the entire page and advance */
        memcpy(pvDst, pvSrc, PAGE_SIZE);
        PGMPhysReleasePageMappingLock(pVM, &Lock);
        GCPhysSrc += PAGE_SIZE;
        pvDst = (uint8_t *)pvDst + PAGE_SIZE;
        cb -= PAGE_SIZE;
    }
    /* won't ever get here. */
}

#ifndef IN_RC /* Ring 0 & 3 only. (Just not needed in GC.) */

/**
 * Write to guest physical memory referenced by GC pointer.
 * Write memory to GC physical address in guest physical memory.
 *
 * This will bypass MMIO and access handlers.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   GCPhysDst   The GC physical address of the destination.
 * @param   pvSrc       The source buffer.
 * @param   cb          The number of bytes to write.
 */
VMMDECL(int) PGMPhysSimpleWriteGCPhys(PVM pVM, RTGCPHYS GCPhysDst, const void *pvSrc, size_t cb)
{
    LogFlow(("PGMPhysSimpleWriteGCPhys: %RGp %zu\n", GCPhysDst, cb));

    /*
     * Treat the first page as a special case.
     */
    if (!cb)
        return VINF_SUCCESS;

    /* map the 1st page */
    void *pvDst;
    PGMPAGEMAPLOCK Lock;
    int rc = PGMPhysGCPhys2CCPtr(pVM, GCPhysDst, &pvDst, &Lock);
    if (RT_FAILURE(rc))
        return rc;

    /* optimize for the case where access is completely within the first page. */
    size_t cbPage = PAGE_SIZE - (GCPhysDst & PAGE_OFFSET_MASK);
    if (RT_LIKELY(cb <= cbPage))
    {
        memcpy(pvDst, pvSrc, cb);
        PGMPhysReleasePageMappingLock(pVM, &Lock);
        return VINF_SUCCESS;
    }

    /* copy to the end of the page. */
    memcpy(pvDst, pvSrc, cbPage);
    PGMPhysReleasePageMappingLock(pVM, &Lock);
    GCPhysDst += cbPage;
    pvSrc = (const uint8_t *)pvSrc + cbPage;
    cb -= cbPage;

    /*
     * Page by page.
     */
    for (;;)
    {
        /* map the page */
        rc = PGMPhysGCPhys2CCPtr(pVM, GCPhysDst, &pvDst, &Lock);
        if (RT_FAILURE(rc))
            return rc;

        /* last page? */
        if (cb <= PAGE_SIZE)
        {
            memcpy(pvDst, pvSrc, cb);
            PGMPhysReleasePageMappingLock(pVM, &Lock);
            return VINF_SUCCESS;
        }

        /* copy the entire page and advance */
        memcpy(pvDst, pvSrc, PAGE_SIZE);
        PGMPhysReleasePageMappingLock(pVM, &Lock);
        GCPhysDst += PAGE_SIZE;
        pvSrc = (const uint8_t *)pvSrc + PAGE_SIZE;
        cb -= PAGE_SIZE;
    }
    /* won't ever get here. */
}


/**
 * Read from guest physical memory referenced by GC pointer.
 *
 * This function uses the current CR3/CR0/CR4 of the guest and will
 * bypass access handlers and not set any accessed bits.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   pvDst       The destination address.
 * @param   GCPtrSrc    The source address (GC pointer).
 * @param   cb          The number of bytes to read.
 */
VMMDECL(int) PGMPhysSimpleReadGCPtr(PVM pVM, void *pvDst, RTGCPTR GCPtrSrc, size_t cb)
{
    /*
     * Treat the first page as a special case.
     */
    if (!cb)
        return VINF_SUCCESS;

    /* map the 1st page */
    void const *pvSrc;
    PGMPAGEMAPLOCK Lock;
    int rc = PGMPhysGCPtr2CCPtrReadOnly(pVM, GCPtrSrc, &pvSrc, &Lock);
    if (RT_FAILURE(rc))
        return rc;

    /* optimize for the case where access is completely within the first page. */
    size_t cbPage = PAGE_SIZE - ((RTGCUINTPTR)GCPtrSrc & PAGE_OFFSET_MASK);
    if (RT_LIKELY(cb <= cbPage))
    {
        memcpy(pvDst, pvSrc, cb);
        PGMPhysReleasePageMappingLock(pVM, &Lock);
        return VINF_SUCCESS;
    }

    /* copy to the end of the page. */
    memcpy(pvDst, pvSrc, cbPage);
    PGMPhysReleasePageMappingLock(pVM, &Lock);
    GCPtrSrc = (RTGCPTR)((RTGCUINTPTR)GCPtrSrc + cbPage);
    pvDst = (uint8_t *)pvDst + cbPage;
    cb -= cbPage;

    /*
     * Page by page.
     */
    for (;;)
    {
        /* map the page */
        rc = PGMPhysGCPtr2CCPtrReadOnly(pVM, GCPtrSrc, &pvSrc, &Lock);
        if (RT_FAILURE(rc))
            return rc;

        /* last page? */
        if (cb <= PAGE_SIZE)
        {
            memcpy(pvDst, pvSrc, cb);
            PGMPhysReleasePageMappingLock(pVM, &Lock);
            return VINF_SUCCESS;
        }

        /* copy the entire page and advance */
        memcpy(pvDst, pvSrc, PAGE_SIZE);
        PGMPhysReleasePageMappingLock(pVM, &Lock);
        GCPtrSrc = (RTGCPTR)((RTGCUINTPTR)GCPtrSrc + PAGE_SIZE);
        pvDst = (uint8_t *)pvDst + PAGE_SIZE;
        cb -= PAGE_SIZE;
    }
    /* won't ever get here. */
}


/**
 * Write to guest physical memory referenced by GC pointer.
 *
 * This function uses the current CR3/CR0/CR4 of the guest and will
 * bypass access handlers and not set dirty or accessed bits.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   GCPtrDst    The destination address (GC pointer).
 * @param   pvSrc       The source address.
 * @param   cb          The number of bytes to write.
 */
VMMDECL(int) PGMPhysSimpleWriteGCPtr(PVM pVM, RTGCPTR GCPtrDst, const void *pvSrc, size_t cb)
{
    /*
     * Treat the first page as a special case.
     */
    if (!cb)
        return VINF_SUCCESS;

    /* map the 1st page */
    void *pvDst;
    PGMPAGEMAPLOCK Lock;
    int rc = PGMPhysGCPtr2CCPtr(pVM, GCPtrDst, &pvDst, &Lock);
    if (RT_FAILURE(rc))
        return rc;

    /* optimize for the case where access is completely within the first page. */
    size_t cbPage = PAGE_SIZE - ((RTGCUINTPTR)GCPtrDst & PAGE_OFFSET_MASK);
    if (RT_LIKELY(cb <= cbPage))
    {
        memcpy(pvDst, pvSrc, cb);
        PGMPhysReleasePageMappingLock(pVM, &Lock);
        return VINF_SUCCESS;
    }

    /* copy to the end of the page. */
    memcpy(pvDst, pvSrc, cbPage);
    PGMPhysReleasePageMappingLock(pVM, &Lock);
    GCPtrDst = (RTGCPTR)((RTGCUINTPTR)GCPtrDst + cbPage);
    pvSrc = (const uint8_t *)pvSrc + cbPage;
    cb -= cbPage;

    /*
     * Page by page.
     */
    for (;;)
    {
        /* map the page */
        rc = PGMPhysGCPtr2CCPtr(pVM, GCPtrDst, &pvDst, &Lock);
        if (RT_FAILURE(rc))
            return rc;

        /* last page? */
        if (cb <= PAGE_SIZE)
        {
            memcpy(pvDst, pvSrc, cb);
            PGMPhysReleasePageMappingLock(pVM, &Lock);
            return VINF_SUCCESS;
        }

        /* copy the entire page and advance */
        memcpy(pvDst, pvSrc, PAGE_SIZE);
        PGMPhysReleasePageMappingLock(pVM, &Lock);
        GCPtrDst = (RTGCPTR)((RTGCUINTPTR)GCPtrDst + PAGE_SIZE);
        pvSrc = (const uint8_t *)pvSrc + PAGE_SIZE;
        cb -= PAGE_SIZE;
    }
    /* won't ever get here. */
}


/**
 * Write to guest physical memory referenced by GC pointer and update the PTE.
 *
 * This function uses the current CR3/CR0/CR4 of the guest and will
 * bypass access handlers but will set any dirty and accessed bits in the PTE.
 *
 * If you don't want to set the dirty bit, use PGMPhysSimpleWriteGCPtr().
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   GCPtrDst    The destination address (GC pointer).
 * @param   pvSrc       The source address.
 * @param   cb          The number of bytes to write.
 */
VMMDECL(int) PGMPhysSimpleDirtyWriteGCPtr(PVM pVM, RTGCPTR GCPtrDst, const void *pvSrc, size_t cb)
{
    /*
     * Treat the first page as a special case.
     * Btw. this is the same code as in PGMPhyssimpleWriteGCPtr excep for the PGMGstModifyPage.
     */
    if (!cb)
        return VINF_SUCCESS;

    /* map the 1st page */
    void *pvDst;
    PGMPAGEMAPLOCK Lock;
    int rc = PGMPhysGCPtr2CCPtr(pVM, GCPtrDst, &pvDst, &Lock);
    if (RT_FAILURE(rc))
        return rc;

    /* optimize for the case where access is completely within the first page. */
    size_t cbPage = PAGE_SIZE - ((RTGCUINTPTR)GCPtrDst & PAGE_OFFSET_MASK);
    if (RT_LIKELY(cb <= cbPage))
    {
        memcpy(pvDst, pvSrc, cb);
        PGMPhysReleasePageMappingLock(pVM, &Lock);
        rc = PGMGstModifyPage(pVM, GCPtrDst, 1, X86_PTE_A | X86_PTE_D, ~(uint64_t)(X86_PTE_A | X86_PTE_D)); AssertRC(rc);
        return VINF_SUCCESS;
    }

    /* copy to the end of the page. */
    memcpy(pvDst, pvSrc, cbPage);
    PGMPhysReleasePageMappingLock(pVM, &Lock);
    rc = PGMGstModifyPage(pVM, GCPtrDst, 1, X86_PTE_A | X86_PTE_D, ~(uint64_t)(X86_PTE_A | X86_PTE_D)); AssertRC(rc);
    GCPtrDst = (RTGCPTR)((RTGCUINTPTR)GCPtrDst + cbPage);
    pvSrc = (const uint8_t *)pvSrc + cbPage;
    cb -= cbPage;

    /*
     * Page by page.
     */
    for (;;)
    {
        /* map the page */
        rc = PGMPhysGCPtr2CCPtr(pVM, GCPtrDst, &pvDst, &Lock);
        if (RT_FAILURE(rc))
            return rc;

        /* last page? */
        if (cb <= PAGE_SIZE)
        {
            memcpy(pvDst, pvSrc, cb);
            PGMPhysReleasePageMappingLock(pVM, &Lock);
            rc = PGMGstModifyPage(pVM, GCPtrDst, 1, X86_PTE_A | X86_PTE_D, ~(uint64_t)(X86_PTE_A | X86_PTE_D)); AssertRC(rc);
            return VINF_SUCCESS;
        }

        /* copy the entire page and advance */
        memcpy(pvDst, pvSrc, PAGE_SIZE);
        PGMPhysReleasePageMappingLock(pVM, &Lock);
        rc = PGMGstModifyPage(pVM, GCPtrDst, 1, X86_PTE_A | X86_PTE_D, ~(uint64_t)(X86_PTE_A | X86_PTE_D)); AssertRC(rc);
        GCPtrDst = (RTGCPTR)((RTGCUINTPTR)GCPtrDst + PAGE_SIZE);
        pvSrc = (const uint8_t *)pvSrc + PAGE_SIZE;
        cb -= PAGE_SIZE;
    }
    /* won't ever get here. */
}


/**
 * Read from guest physical memory referenced by GC pointer.
 *
 * This function uses the current CR3/CR0/CR4 of the guest and will
 * respect access handlers and set accessed bits.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   pvDst       The destination address.
 * @param   GCPtrSrc    The source address (GC pointer).
 * @param   cb          The number of bytes to read.
 * @thread  The vCPU EMT.
 */
VMMDECL(int) PGMPhysReadGCPtr(PVM pVM, void *pvDst, RTGCPTR GCPtrSrc, size_t cb)
{
    RTGCPHYS    GCPhys;
    uint64_t    fFlags;
    int         rc;

    /*
     * Anything to do?
     */
    if (!cb)
        return VINF_SUCCESS;

    LogFlow(("PGMPhysReadGCPtr: %RGv %zu\n", GCPtrSrc, cb));

    /*
     * Optimize reads within a single page.
     */
    if (((RTGCUINTPTR)GCPtrSrc & PAGE_OFFSET_MASK) + cb <= PAGE_SIZE)
    {
        /* Convert virtual to physical address + flags */
        rc = PGM_GST_PFN(GetPage,pVM)(pVM, (RTGCUINTPTR)GCPtrSrc, &fFlags, &GCPhys);
        AssertMsgRCReturn(rc, ("GetPage failed with %Rrc for %RGv\n", rc, GCPtrSrc), rc);
        GCPhys |= (RTGCUINTPTR)GCPtrSrc & PAGE_OFFSET_MASK;

        /* mark the guest page as accessed. */
        if (!(fFlags & X86_PTE_A))
        {
            rc = PGMGstModifyPage(pVM, GCPtrSrc, 1, X86_PTE_A, ~(uint64_t)(X86_PTE_A));
            AssertRC(rc);
        }

#ifdef VBOX_WITH_NEW_PHYS_CODE
        return PGMPhysRead(pVM, GCPhys, pvDst, cb);
#else
        PGMPhysRead(pVM, GCPhys, pvDst, cb);
        return VINF_SUCCESS;
#endif
    }

    /*
     * Page by page.
     */
    for (;;)
    {
        /* Convert virtual to physical address + flags */
        rc = PGM_GST_PFN(GetPage,pVM)(pVM, (RTGCUINTPTR)GCPtrSrc, &fFlags, &GCPhys);
        AssertMsgRCReturn(rc, ("GetPage failed with %Rrc for %RGv\n", rc, GCPtrSrc), rc);
        GCPhys |= (RTGCUINTPTR)GCPtrSrc & PAGE_OFFSET_MASK;

        /* mark the guest page as accessed. */
        if (!(fFlags & X86_PTE_A))
        {
            rc = PGMGstModifyPage(pVM, GCPtrSrc, 1, X86_PTE_A, ~(uint64_t)(X86_PTE_A));
            AssertRC(rc);
        }

        /* copy */
        size_t cbRead = PAGE_SIZE - ((RTGCUINTPTR)GCPtrSrc & PAGE_OFFSET_MASK);
#ifdef VBOX_WITH_NEW_PHYS_CODE
        rc = PGMPhysRead(pVM, GCPhys, pvDst, cbRead);
        if (cbRead >= cb || RT_FAILURE(rc))
            return rc;
#else
        if (cbRead >= cb)
        {
            PGMPhysRead(pVM, GCPhys, pvDst, cb);
            return VINF_SUCCESS;
        }
        PGMPhysRead(pVM, GCPhys, pvDst, cbRead);
#endif

        /* next */
        cb         -= cbRead;
        pvDst       = (uint8_t *)pvDst + cbRead;
        GCPtrSrc   += cbRead;
    }
}


/**
 * Write to guest physical memory referenced by GC pointer.
 *
 * This function uses the current CR3/CR0/CR4 of the guest and will
 * respect access handlers and set dirty and accessed bits.
 *
 * @returns VBox status.
 * @retval  VINF_SUCCESS.
 * @retval  VERR_PGM_PHYS_WR_HIT_HANDLER in R0 and GC, NEVER in R3.
 *
 * @param   pVM         VM handle.
 * @param   GCPtrDst    The destination address (GC pointer).
 * @param   pvSrc       The source address.
 * @param   cb          The number of bytes to write.
 */
VMMDECL(int) PGMPhysWriteGCPtr(PVM pVM, RTGCPTR GCPtrDst, const void *pvSrc, size_t cb)
{
    RTGCPHYS    GCPhys;
    uint64_t    fFlags;
    int         rc;

    /*
     * Anything to do?
     */
    if (!cb)
        return VINF_SUCCESS;

    LogFlow(("PGMPhysWriteGCPtr: %RGv %zu\n", GCPtrDst, cb));

    /*
     * Optimize writes within a single page.
     */
    if (((RTGCUINTPTR)GCPtrDst & PAGE_OFFSET_MASK) + cb <= PAGE_SIZE)
    {
        /* Convert virtual to physical address + flags */
        rc = PGM_GST_PFN(GetPage,pVM)(pVM, (RTGCUINTPTR)GCPtrDst, &fFlags, &GCPhys);
        AssertMsgRCReturn(rc, ("GetPage failed with %Rrc for %RGv\n", rc, GCPtrDst), rc);
        GCPhys |= (RTGCUINTPTR)GCPtrDst & PAGE_OFFSET_MASK;

        /* Mention when we ignore X86_PTE_RW... */
        if (!(fFlags & X86_PTE_RW))
            Log(("PGMPhysGCPtr2GCPhys: Writing to RO page %RGv %#x\n", GCPtrDst, cb));

        /* Mark the guest page as accessed and dirty if necessary. */
        if ((fFlags & (X86_PTE_A | X86_PTE_D)) != (X86_PTE_A | X86_PTE_D))
        {
            rc = PGMGstModifyPage(pVM, GCPtrDst, 1, X86_PTE_A | X86_PTE_D, ~(uint64_t)(X86_PTE_A | X86_PTE_D));
            AssertRC(rc);
        }

#ifdef VBOX_WITH_NEW_PHYS_CODE
        return PGMPhysWrite(pVM, GCPhys, pvSrc, cb);
#else
        PGMPhysWrite(pVM, GCPhys, pvSrc, cb);
        return VINF_SUCCESS;
#endif
    }

    /*
     * Page by page.
     */
    for (;;)
    {
        /* Convert virtual to physical address + flags */
        rc = PGM_GST_PFN(GetPage,pVM)(pVM, (RTGCUINTPTR)GCPtrDst, &fFlags, &GCPhys);
        AssertMsgRCReturn(rc, ("GetPage failed with %Rrc for %RGv\n", rc, GCPtrDst), rc);
        GCPhys |= (RTGCUINTPTR)GCPtrDst & PAGE_OFFSET_MASK;

        /* Mention when we ignore X86_PTE_RW... */
        if (!(fFlags & X86_PTE_RW))
            Log(("PGMPhysGCPtr2GCPhys: Writing to RO page %RGv %#x\n", GCPtrDst, cb));

        /* Mark the guest page as accessed and dirty if necessary. */
        if ((fFlags & (X86_PTE_A | X86_PTE_D)) != (X86_PTE_A | X86_PTE_D))
        {
            rc = PGMGstModifyPage(pVM, GCPtrDst, 1, X86_PTE_A | X86_PTE_D, ~(uint64_t)(X86_PTE_A | X86_PTE_D));
            AssertRC(rc);
        }

        /* copy */
        size_t cbWrite = PAGE_SIZE - ((RTGCUINTPTR)GCPtrDst & PAGE_OFFSET_MASK);
#ifdef VBOX_WITH_NEW_PHYS_CODE
        int rc = PGMPhysWrite(pVM, GCPhys, pvSrc, cbWrite);
        if (cbWrite >= cb || RT_FAILURE(rc))
            return rc;
#else
        if (cbWrite >= cb)
        {
            PGMPhysWrite(pVM, GCPhys, pvSrc, cb);
            return VINF_SUCCESS;
        }
        PGMPhysWrite(pVM, GCPhys, pvSrc, cbWrite);
#endif

        /* next */
        cb         -= cbWrite;
        pvSrc       = (uint8_t *)pvSrc + cbWrite;
        GCPtrDst   += cbWrite;
    }
}

#endif /* !IN_RC */

/**
 * Performs a read of guest virtual memory for instruction emulation.
 *
 * This will check permissions, raise exceptions and update the access bits.
 *
 * The current implementation will bypass all access handlers. It may later be
 * changed to at least respect MMIO.
 *
 *
 * @returns VBox status code suitable to scheduling.
 * @retval  VINF_SUCCESS if the read was performed successfully.
 * @retval  VINF_EM_RAW_GUEST_TRAP if an exception was raised but not dispatched yet.
 * @retval  VINF_TRPM_XCPT_DISPATCHED if an exception was raised and dispatched.
 *
 * @param   pVM         The VM handle.
 * @param   pCtxCore    The context core.
 * @param   pvDst       Where to put the bytes we've read.
 * @param   GCPtrSrc    The source address.
 * @param   cb          The number of bytes to read. Not more than a page.
 *
 * @remark  This function will dynamically map physical pages in GC. This may unmap
 *          mappings done by the caller. Be careful!
 */
VMMDECL(int) PGMPhysInterpretedRead(PVM pVM, PCPUMCTXCORE pCtxCore, void *pvDst, RTGCUINTPTR GCPtrSrc, size_t cb)
{
    Assert(cb <= PAGE_SIZE);

/** @todo r=bird: This isn't perfect!
 *  -# It's not checking for reserved bits being 1.
 *  -# It's not correctly dealing with the access bit.
 *  -# It's not respecting MMIO memory or any other access handlers.
 */
    /*
     * 1. Translate virtual to physical. This may fault.
     * 2. Map the physical address.
     * 3. Do the read operation.
     * 4. Set access bits if required.
     */
    int rc;
    unsigned cb1 = PAGE_SIZE - (GCPtrSrc & PAGE_OFFSET_MASK);
    if (cb <= cb1)
    {
        /*
         * Not crossing pages.
         */
        RTGCPHYS GCPhys;
        uint64_t fFlags;
        rc = PGM_GST_PFN(GetPage,pVM)(pVM, GCPtrSrc, &fFlags, &GCPhys);
        if (RT_SUCCESS(rc))
        {
            /** @todo we should check reserved bits ... */
            void *pvSrc;
            rc = PGM_GCPHYS_2_PTR(pVM, GCPhys, &pvSrc);
            switch (rc)
            {
                case VINF_SUCCESS:
                    Log(("PGMPhysInterpretedRead: pvDst=%p pvSrc=%p cb=%d\n", pvDst, (uint8_t *)pvSrc + (GCPtrSrc & PAGE_OFFSET_MASK), cb));
                    memcpy(pvDst, (uint8_t *)pvSrc + (GCPtrSrc & PAGE_OFFSET_MASK), cb);
                    break;
                case VERR_PGM_PHYS_PAGE_RESERVED:
                case VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS:
                    memset(pvDst, 0, cb); /** @todo this is wrong, it should be 0xff */
                    break;
                default:
                    return rc;
            }

            /** @todo access bit emulation isn't 100% correct. */
            if (!(fFlags & X86_PTE_A))
            {
                rc = PGM_GST_PFN(ModifyPage,pVM)(pVM, GCPtrSrc, 1, X86_PTE_A, ~(uint64_t)X86_PTE_A);
                AssertRC(rc);
            }
            return VINF_SUCCESS;
        }
    }
    else
    {
        /*
         * Crosses pages.
         */
        size_t cb2 = cb - cb1;
        uint64_t fFlags1;
        RTGCPHYS GCPhys1;
        uint64_t fFlags2;
        RTGCPHYS GCPhys2;
        rc = PGM_GST_PFN(GetPage,pVM)(pVM, GCPtrSrc, &fFlags1, &GCPhys1);
        if (RT_SUCCESS(rc))
            rc = PGM_GST_PFN(GetPage,pVM)(pVM, GCPtrSrc + cb1, &fFlags2, &GCPhys2);
        if (RT_SUCCESS(rc))
        {
            /** @todo we should check reserved bits ... */
            AssertMsgFailed(("cb=%d cb1=%d cb2=%d GCPtrSrc=%RGv\n", cb, cb1, cb2, GCPtrSrc));
            void *pvSrc1;
            rc = PGM_GCPHYS_2_PTR(pVM, GCPhys1, &pvSrc1);
            switch (rc)
            {
                case VINF_SUCCESS:
                    memcpy(pvDst, (uint8_t *)pvSrc1 + (GCPtrSrc & PAGE_OFFSET_MASK), cb1);
                    break;
                case VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS:
                    memset(pvDst, 0, cb1); /** @todo this is wrong, it should be 0xff */
                    break;
                default:
                    return rc;
            }

            void *pvSrc2;
            rc = PGM_GCPHYS_2_PTR(pVM, GCPhys2, &pvSrc2);
            switch (rc)
            {
                case VINF_SUCCESS:
                    memcpy((uint8_t *)pvDst + cb1, pvSrc2, cb2);
                    break;
                case VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS:
                    memset((uint8_t *)pvDst + cb1, 0, cb2);  /** @todo this is wrong, it should be 0xff */
                    break;
                default:
                    return rc;
            }

            if (!(fFlags1 & X86_PTE_A))
            {
                rc = PGM_GST_PFN(ModifyPage,pVM)(pVM, GCPtrSrc, 1, X86_PTE_A, ~(uint64_t)X86_PTE_A);
                AssertRC(rc);
            }
            if (!(fFlags2 & X86_PTE_A))
            {
                rc = PGM_GST_PFN(ModifyPage,pVM)(pVM, GCPtrSrc + cb1, 1, X86_PTE_A, ~(uint64_t)X86_PTE_A);
                AssertRC(rc);
            }
            return VINF_SUCCESS;
        }
    }

    /*
     * Raise a #PF.
     */
    uint32_t uErr;

    /* Get the current privilege level. */
    uint32_t cpl = CPUMGetGuestCPL(pVM, pCtxCore);
    switch (rc)
    {
        case VINF_SUCCESS:
            uErr = (cpl >= 2) ? X86_TRAP_PF_RSVD | X86_TRAP_PF_US : X86_TRAP_PF_RSVD;
            break;

        case VERR_PAGE_NOT_PRESENT:
        case VERR_PAGE_TABLE_NOT_PRESENT:
            uErr = (cpl >= 2) ? X86_TRAP_PF_US : 0;
            break;

        default:
            AssertMsgFailed(("rc=%Rrc GCPtrSrc=%RGv cb=%#x\n", rc, GCPtrSrc, cb));
            return rc;
    }
    Log(("PGMPhysInterpretedRead: GCPtrSrc=%RGv cb=%#x -> #PF(%#x)\n", GCPtrSrc, cb, uErr));
    return TRPMRaiseXcptErrCR2(pVM, pCtxCore, X86_XCPT_PF, uErr, GCPtrSrc);
}


/**
 * Performs a read of guest virtual memory for instruction emulation.
 *
 * This will check permissions, raise exceptions and update the access bits.
 *
 * The current implementation will bypass all access handlers. It may later be
 * changed to at least respect MMIO.
 *
 *
 * @returns VBox status code suitable to scheduling.
 * @retval  VINF_SUCCESS if the read was performed successfully.
 * @retval  VINF_EM_RAW_GUEST_TRAP if an exception was raised but not dispatched yet.
 * @retval  VINF_TRPM_XCPT_DISPATCHED if an exception was raised and dispatched.
 *
 * @param   pVM         The VM handle.
 * @param   pCtxCore    The context core.
 * @param   pvDst       Where to put the bytes we've read.
 * @param   GCPtrSrc    The source address.
 * @param   cb          The number of bytes to read. Not more than a page.
 * @param   fRaiseTrap  If set the trap will be raised on as per spec, if clear
 *                      an appropriate error status will be returned (no
 *                      informational at all).
 *
 *
 * @remarks Takes the PGM lock.
 * @remarks A page fault on the 2nd page of the access will be raised without
 *          writing the bits on the first page since we're ASSUMING that the
 *          caller is emulating an instruction access.
 * @remarks This function will dynamically map physical pages in GC. This may
 *          unmap mappings done by the caller. Be careful!
 */
VMMDECL(int) PGMPhysInterpretedReadNoHandlers(PVM pVM, PCPUMCTXCORE pCtxCore, void *pvDst, RTGCUINTPTR GCPtrSrc, size_t cb, bool fRaiseTrap)
{
    Assert(cb <= PAGE_SIZE);

    /*
     * 1. Translate virtual to physical. This may fault.
     * 2. Map the physical address.
     * 3. Do the read operation.
     * 4. Set access bits if required.
     */
    int rc;
    unsigned cb1 = PAGE_SIZE - (GCPtrSrc & PAGE_OFFSET_MASK);
    if (cb <= cb1)
    {
        /*
         * Not crossing pages.
         */
        RTGCPHYS    GCPhys;
        uint64_t    fFlags;
        rc = PGM_GST_PFN(GetPage,pVM)(pVM, GCPtrSrc, &fFlags, &GCPhys);
        if (RT_SUCCESS(rc))
        {
            if (1) /** @todo we should check reserved bits ... */
            {
                const void *pvSrc;
                PGMPAGEMAPLOCK Lock;
                rc = PGMPhysGCPhys2CCPtrReadOnly(pVM, GCPhys, &pvSrc, &Lock);
                switch (rc)
                {
                    case VINF_SUCCESS:
                        Log(("PGMPhysInterpretedReadNoHandlers: pvDst=%p pvSrc=%p (%RGv) cb=%d\n",
                               pvDst, (const uint8_t *)pvSrc + (GCPtrSrc & PAGE_OFFSET_MASK), GCPtrSrc, cb));
                        memcpy(pvDst, (const uint8_t *)pvSrc + (GCPtrSrc & PAGE_OFFSET_MASK), cb);
                        break;
                    case VERR_PGM_PHYS_PAGE_RESERVED:
                    case VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS:
                        memset(pvDst, 0xff, cb);
                        break;
                    default:
                        AssertMsgFailed(("%Rrc\n", rc));
                        AssertReturn(RT_FAILURE(rc), VERR_IPE_UNEXPECTED_INFO_STATUS);
                        return rc;
                }
                PGMPhysReleasePageMappingLock(pVM, &Lock);

                if (!(fFlags & X86_PTE_A))
                {
                    /** @todo access bit emulation isn't 100% correct. */
                    rc = PGM_GST_PFN(ModifyPage,pVM)(pVM, GCPtrSrc, 1, X86_PTE_A, ~(uint64_t)X86_PTE_A);
                    AssertRC(rc);
                }
                return VINF_SUCCESS;
            }
        }
    }
    else
    {
        /*
         * Crosses pages.
         */
        size_t      cb2 = cb - cb1;
        uint64_t    fFlags1;
        RTGCPHYS    GCPhys1;
        uint64_t    fFlags2;
        RTGCPHYS    GCPhys2;
        rc = PGM_GST_PFN(GetPage,pVM)(pVM, GCPtrSrc, &fFlags1, &GCPhys1);
        if (RT_SUCCESS(rc))
        {
            rc = PGM_GST_PFN(GetPage,pVM)(pVM, GCPtrSrc + cb1, &fFlags2, &GCPhys2);
            if (RT_SUCCESS(rc))
            {
                if (1) /** @todo we should check reserved bits ... */
                {
                    const void *pvSrc;
                    PGMPAGEMAPLOCK Lock;
                    rc = PGMPhysGCPhys2CCPtrReadOnly(pVM, GCPhys1, &pvSrc, &Lock);
                    switch (rc)
                    {
                        case VINF_SUCCESS:
                            Log(("PGMPhysInterpretedReadNoHandlers: pvDst=%p pvSrc=%p (%RGv) cb=%d [2]\n",
                                   pvDst, (const uint8_t *)pvSrc + (GCPtrSrc & PAGE_OFFSET_MASK), GCPtrSrc, cb1));
                            memcpy(pvDst, (const uint8_t *)pvSrc + (GCPtrSrc & PAGE_OFFSET_MASK), cb1);
                            PGMPhysReleasePageMappingLock(pVM, &Lock);
                            break;
                        case VERR_PGM_PHYS_PAGE_RESERVED:
                        case VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS:
                            memset(pvDst, 0xff, cb1);
                            break;
                        default:
                            AssertMsgFailed(("%Rrc\n", rc));
                            AssertReturn(RT_FAILURE(rc), VERR_IPE_UNEXPECTED_INFO_STATUS);
                            return rc;
                    }

                    rc = PGMPhysGCPhys2CCPtrReadOnly(pVM, GCPhys2, &pvSrc, &Lock);
                    switch (rc)
                    {
                        case VINF_SUCCESS:
                            memcpy((uint8_t *)pvDst + cb1, pvSrc, cb2);
                            PGMPhysReleasePageMappingLock(pVM, &Lock);
                            break;
                        case VERR_PGM_PHYS_PAGE_RESERVED:
                        case VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS:
                            memset((uint8_t *)pvDst + cb1, 0xff, cb2);
                            break;
                        default:
                            AssertMsgFailed(("%Rrc\n", rc));
                            AssertReturn(RT_FAILURE(rc), VERR_IPE_UNEXPECTED_INFO_STATUS);
                            return rc;
                    }

                    if (!(fFlags1 & X86_PTE_A))
                    {
                        rc = PGM_GST_PFN(ModifyPage,pVM)(pVM, GCPtrSrc, 1, X86_PTE_A, ~(uint64_t)X86_PTE_A);
                        AssertRC(rc);
                    }
                    if (!(fFlags2 & X86_PTE_A))
                    {
                        rc = PGM_GST_PFN(ModifyPage,pVM)(pVM, GCPtrSrc + cb1, 1, X86_PTE_A, ~(uint64_t)X86_PTE_A);
                        AssertRC(rc);
                    }
                    return VINF_SUCCESS;
                }
                /* sort out which page */
            }
            else
                GCPtrSrc += cb1; /* fault on 2nd page */
        }
    }

    /*
     * Raise a #PF if we're allowed to do that.
     */
    /* Calc the error bits. */
    uint32_t cpl = CPUMGetGuestCPL(pVM, pCtxCore);
    uint32_t uErr;
    switch (rc)
    {
        case VINF_SUCCESS:
            uErr = (cpl >= 2) ? X86_TRAP_PF_RSVD | X86_TRAP_PF_US : X86_TRAP_PF_RSVD;
            rc = VERR_ACCESS_DENIED;
            break;

        case VERR_PAGE_NOT_PRESENT:
        case VERR_PAGE_TABLE_NOT_PRESENT:
            uErr = (cpl >= 2) ? X86_TRAP_PF_US : 0;
            break;

        default:
            AssertMsgFailed(("rc=%Rrc GCPtrSrc=%RGv cb=%#x\n", rc, GCPtrSrc, cb));
            AssertReturn(RT_FAILURE(rc), VERR_IPE_UNEXPECTED_INFO_STATUS);
            return rc;
    }
    if (fRaiseTrap)
    {
        Log(("PGMPhysInterpretedReadNoHandlers: GCPtrSrc=%RGv cb=%#x -> Raised #PF(%#x)\n", GCPtrSrc, cb, uErr));
        return TRPMRaiseXcptErrCR2(pVM, pCtxCore, X86_XCPT_PF, uErr, GCPtrSrc);
    }
    Log(("PGMPhysInterpretedReadNoHandlers: GCPtrSrc=%RGv cb=%#x -> #PF(%#x) [!raised]\n", GCPtrSrc, cb, uErr));
    return rc;
}


/**
 * Performs a write to guest virtual memory for instruction emulation.
 *
 * This will check permissions, raise exceptions and update the dirty and access
 * bits.
 *
 * @returns VBox status code suitable to scheduling.
 * @retval  VINF_SUCCESS if the read was performed successfully.
 * @retval  VINF_EM_RAW_GUEST_TRAP if an exception was raised but not dispatched yet.
 * @retval  VINF_TRPM_XCPT_DISPATCHED if an exception was raised and dispatched.
 *
 * @param   pVM         The VM handle.
 * @param   pCtxCore    The context core.
 * @param   GCPtrDst    The destination address.
 * @param   pvSrc       What to write.
 * @param   cb          The number of bytes to write. Not more than a page.
 * @param   fRaiseTrap  If set the trap will be raised on as per spec, if clear
 *                      an appropriate error status will be returned (no
 *                      informational at all).
 *
 * @remarks Takes the PGM lock.
 * @remarks A page fault on the 2nd page of the access will be raised without
 *          writing the bits on the first page since we're ASSUMING that the
 *          caller is emulating an instruction access.
 * @remarks This function will dynamically map physical pages in GC. This may
 *          unmap mappings done by the caller. Be careful!
 */
VMMDECL(int) PGMPhysInterpretedWriteNoHandlers(PVM pVM, PCPUMCTXCORE pCtxCore, RTGCPTR GCPtrDst, const void *pvSrc, size_t cb, bool fRaiseTrap)
{
    Assert(cb <= PAGE_SIZE);

    /*
     * 1. Translate virtual to physical. This may fault.
     * 2. Map the physical address.
     * 3. Do the write operation.
     * 4. Set access bits if required.
     */
    int rc;
    unsigned cb1 = PAGE_SIZE - (GCPtrDst & PAGE_OFFSET_MASK);
    if (cb <= cb1)
    {
        /*
         * Not crossing pages.
         */
        RTGCPHYS    GCPhys;
        uint64_t    fFlags;
        rc = PGM_GST_PFN(GetPage,pVM)(pVM, GCPtrDst, &fFlags, &GCPhys);
        if (RT_SUCCESS(rc))
        {
            if (    (fFlags & X86_PTE_RW)                   /** @todo Also check reserved bits. */
                ||  (   !(CPUMGetGuestCR0(pVM) & X86_CR0_WP)
                     &&   CPUMGetGuestCPL(pVM, pCtxCore) <= 2) ) /** @todo it's 2, right? Check cpl check below as well. */
            {
                void *pvDst;
                PGMPAGEMAPLOCK Lock;
                rc = PGMPhysGCPhys2CCPtr(pVM, GCPhys, &pvDst, &Lock);
                switch (rc)
                {
                    case VINF_SUCCESS:
                        Log(("PGMPhysInterpretedWriteNoHandlers: pvDst=%p (%RGv) pvSrc=%p cb=%d\n",
                               (uint8_t *)pvDst + (GCPtrDst & PAGE_OFFSET_MASK), GCPtrDst, pvSrc,  cb));
                        memcpy((uint8_t *)pvDst + (GCPtrDst & PAGE_OFFSET_MASK), pvSrc, cb);
                        PGMPhysReleasePageMappingLock(pVM, &Lock);
                        break;
                    case VERR_PGM_PHYS_PAGE_RESERVED:
                    case VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS:
                        /* bit bucket */
                        break;
                    default:
                        AssertMsgFailed(("%Rrc\n", rc));
                        AssertReturn(RT_FAILURE(rc), VERR_IPE_UNEXPECTED_INFO_STATUS);
                        return rc;
                }

                if (!(fFlags & (X86_PTE_A | X86_PTE_D)))
                {
                    /** @todo dirty & access bit emulation isn't 100% correct. */
                    rc = PGM_GST_PFN(ModifyPage,pVM)(pVM, GCPtrDst, 1, X86_PTE_A | X86_PTE_D, ~(uint64_t)(X86_PTE_A | X86_PTE_D));
                    AssertRC(rc);
                }
                return VINF_SUCCESS;
            }
            rc = VERR_ACCESS_DENIED;
        }
    }
    else
    {
        /*
         * Crosses pages.
         */
        size_t      cb2 = cb - cb1;
        uint64_t    fFlags1;
        RTGCPHYS    GCPhys1;
        uint64_t    fFlags2;
        RTGCPHYS    GCPhys2;
        rc = PGM_GST_PFN(GetPage,pVM)(pVM, GCPtrDst, &fFlags1, &GCPhys1);
        if (RT_SUCCESS(rc))
        {
            rc = PGM_GST_PFN(GetPage,pVM)(pVM, GCPtrDst + cb1, &fFlags2, &GCPhys2);
            if (RT_SUCCESS(rc))
            {
                if (    (   (fFlags1 & X86_PTE_RW)  /** @todo Also check reserved bits. */
                         && (fFlags2 & X86_PTE_RW))
                    ||  (   !(CPUMGetGuestCR0(pVM) & X86_CR0_WP)
                         &&   CPUMGetGuestCPL(pVM, pCtxCore) <= 2) )
                {
                    void *pvDst;
                    PGMPAGEMAPLOCK Lock;
                    rc = PGMPhysGCPhys2CCPtr(pVM, GCPhys1, &pvDst, &Lock);
                    switch (rc)
                    {
                        case VINF_SUCCESS:
                            Log(("PGMPhysInterpretedWriteNoHandlers: pvDst=%p (%RGv) pvSrc=%p cb=%d\n",
                                   (uint8_t *)pvDst + (GCPtrDst & PAGE_OFFSET_MASK), GCPtrDst, pvSrc, cb1));
                            memcpy((uint8_t *)pvDst + (GCPtrDst & PAGE_OFFSET_MASK), pvSrc, cb1);
                            PGMPhysReleasePageMappingLock(pVM, &Lock);
                            break;
                        case VERR_PGM_PHYS_PAGE_RESERVED:
                        case VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS:
                            /* bit bucket */
                            break;
                        default:
                            AssertMsgFailed(("%Rrc\n", rc));
                            AssertReturn(RT_FAILURE(rc), VERR_IPE_UNEXPECTED_INFO_STATUS);
                            return rc;
                    }

                    rc = PGMPhysGCPhys2CCPtr(pVM, GCPhys2, &pvDst, &Lock);
                    switch (rc)
                    {
                        case VINF_SUCCESS:
                            memcpy(pvDst, (const uint8_t *)pvSrc + cb1, cb2);
                            PGMPhysReleasePageMappingLock(pVM, &Lock);
                            break;
                        case VERR_PGM_PHYS_PAGE_RESERVED:
                        case VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS:
                            /* bit bucket */
                            break;
                        default:
                            AssertMsgFailed(("%Rrc\n", rc));
                            AssertReturn(RT_FAILURE(rc), VERR_IPE_UNEXPECTED_INFO_STATUS);
                            return rc;
                    }

                    if (!(fFlags1 & (X86_PTE_A | X86_PTE_RW)))
                    {
                        rc = PGM_GST_PFN(ModifyPage,pVM)(pVM, GCPtrDst, 1, (X86_PTE_A | X86_PTE_RW), ~(uint64_t)(X86_PTE_A | X86_PTE_RW));
                        AssertRC(rc);
                    }
                    if (!(fFlags2 & (X86_PTE_A | X86_PTE_RW)))
                    {
                        rc = PGM_GST_PFN(ModifyPage,pVM)(pVM, GCPtrDst + cb1, 1, (X86_PTE_A | X86_PTE_RW), ~(uint64_t)(X86_PTE_A | X86_PTE_RW));
                        AssertRC(rc);
                    }
                    return VINF_SUCCESS;
                }
                if ((fFlags1 & (X86_PTE_RW)) == X86_PTE_RW)
                    GCPtrDst += cb1; /* fault on the 2nd page. */
                rc = VERR_ACCESS_DENIED;
            }
            else
                GCPtrDst += cb1; /* fault on the 2nd page. */
        }
    }

    /*
     * Raise a #PF if we're allowed to do that.
     */
    /* Calc the error bits. */
    uint32_t uErr;
    uint32_t cpl = CPUMGetGuestCPL(pVM, pCtxCore);
    switch (rc)
    {
        case VINF_SUCCESS:
            uErr = (cpl >= 2) ? X86_TRAP_PF_RSVD | X86_TRAP_PF_US : X86_TRAP_PF_RSVD;
            rc = VERR_ACCESS_DENIED;
            break;

        case VERR_ACCESS_DENIED:
            uErr = (cpl >= 2) ? X86_TRAP_PF_RW | X86_TRAP_PF_US : X86_TRAP_PF_RW;
            break;

        case VERR_PAGE_NOT_PRESENT:
        case VERR_PAGE_TABLE_NOT_PRESENT:
            uErr = (cpl >= 2) ? X86_TRAP_PF_US : 0;
            break;

        default:
            AssertMsgFailed(("rc=%Rrc GCPtrDst=%RGv cb=%#x\n", rc, GCPtrDst, cb));
            AssertReturn(RT_FAILURE(rc), VERR_IPE_UNEXPECTED_INFO_STATUS);
            return rc;
    }
    if (fRaiseTrap)
    {
        Log(("PGMPhysInterpretedWriteNoHandlers: GCPtrDst=%RGv cb=%#x -> Raised #PF(%#x)\n", GCPtrDst, cb, uErr));
        return TRPMRaiseXcptErrCR2(pVM, pCtxCore, X86_XCPT_PF, uErr, GCPtrDst);
    }
    Log(("PGMPhysInterpretedWriteNoHandlers: GCPtrDst=%RGv cb=%#x -> #PF(%#x) [!raised]\n", GCPtrDst, cb, uErr));
    return rc;
}


