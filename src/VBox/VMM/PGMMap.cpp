/* $Id$ */
/** @file
 * PGM - Page Manager, Guest Context Mappings.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_PGM
#include <VBox/dbgf.h>
#include <VBox/pgm.h>
#include "PGMInternal.h"
#include <VBox/vm.h>

#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/string.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void              pgmR3MapClearPDEs(PPGM pPGM, PPGMMAPPING pMap, int iOldPDE);
static void              pgmR3MapSetPDEs(PVM pVM, PPGMMAPPING pMap, int iNewPDE);
static int               pgmR3MapIntermediateCheckOne(PVM pVM, uintptr_t uAddress, unsigned cPages, PX86PT pPTDefault, PX86PTPAE pPTPaeDefault);
static void              pgmR3MapIntermediateDoOne(PVM pVM, uintptr_t uAddress, RTHCPHYS HCPhys, unsigned cPages, PX86PT pPTDefault, PX86PTPAE pPTPaeDefault);



/**
 * Creates a page table based mapping in GC.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   GCPtr           Virtual Address. (Page table aligned!)
 * @param   cb              Size of the range. Must be a 4MB aligned!
 * @param   pfnRelocate     Relocation callback function.
 * @param   pvUser          User argument to the callback.
 * @param   pszDesc         Pointer to description string. This must not be freed.
 */
PGMR3DECL(int) PGMR3MapPT(PVM pVM, RTGCPTR GCPtr, uint32_t cb, PFNPGMRELOCATE pfnRelocate, void *pvUser, const char *pszDesc)
{
    LogFlow(("PGMR3MapPT: GCPtr=%#x cb=%d pfnRelocate=%p pvUser=%p pszDesc=%s\n", GCPtr, cb, pfnRelocate, pvUser, pszDesc));
    AssertMsg(pVM->pgm.s.pInterPD && pVM->pgm.s.pHC32BitPD, ("Paging isn't initialized, init order problems!\n"));

    /*
     * Validate input.
     */
    if (cb < _2M || cb > 64 * _1M)
    {
        AssertMsgFailed(("Serious? cb=%d\n", cb));
        return VERR_INVALID_PARAMETER;
    }
    cb = RT_ALIGN_32(cb, _4M);
    RTGCPTR GCPtrLast = GCPtr + cb - 1;
    if (GCPtrLast < GCPtr)
    {
        AssertMsgFailed(("Range wraps! GCPtr=%x GCPtrLast=%x\n", GCPtr, GCPtrLast));
        return VERR_INVALID_PARAMETER;
    }
    if (pVM->pgm.s.fMappingsFixed)
    {
        AssertMsgFailed(("Mappings are fixed! It's not possible to add new mappings at this time!\n"));
        return VERR_PGM_MAPPINGS_FIXED;
    }
    if (!pfnRelocate)
    {
        AssertMsgFailed(("Callback is required\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Find list location.
     */
    PPGMMAPPING pPrev = NULL;
    PPGMMAPPING pCur = pVM->pgm.s.pMappingsR3;
    while (pCur)
    {
        if (pCur->GCPtrLast >= GCPtr && pCur->GCPtr <= GCPtrLast)
        {
            AssertMsgFailed(("Address is already in use by %s. req %#x-%#x take %#x-%#x\n",
                             pCur->pszDesc, GCPtr, GCPtrLast, pCur->GCPtr, pCur->GCPtrLast));
            LogRel(("VERR_PGM_MAPPING_CONFLICT: Address is already in use by %s. req %#x-%#x take %#x-%#x\n",
                    pCur->pszDesc, GCPtr, GCPtrLast, pCur->GCPtr, pCur->GCPtrLast));
            return VERR_PGM_MAPPING_CONFLICT;
        }
        if (pCur->GCPtr > GCPtr)
            break;
        pPrev = pCur;
        pCur = pCur->pNextR3;
    }

    /*
     * Check for conflicts with intermediate mappings.
     */
    const unsigned iPageDir = GCPtr >> X86_PD_SHIFT;
    const unsigned cPTs = cb >> X86_PD_SHIFT;
    unsigned    i;
    for (i = 0; i < cPTs; i++)
    {
        if (pVM->pgm.s.pInterPD->a[iPageDir + i].n.u1Present)
        {
            AssertMsgFailed(("Address %#x is already in use by an intermediate mapping.\n", GCPtr + (i << PAGE_SHIFT)));
            LogRel(("VERR_PGM_MAPPING_CONFLICT: Address %#x is already in use by an intermediate mapping.\n", GCPtr + (i << PAGE_SHIFT)));
            return VERR_PGM_MAPPING_CONFLICT;
        }
    }
    /** @todo AMD64: add check in PAE structures too, so we can remove all the 32-Bit paging stuff there. */

    /*
     * Allocate and initialize the new list node.
     */
    PPGMMAPPING pNew;
    int rc = MMHyperAlloc(pVM, RT_OFFSETOF(PGMMAPPING, aPTs[cPTs]), 0, MM_TAG_PGM, (void **)&pNew);
    if (VBOX_FAILURE(rc))
        return rc;
    pNew->GCPtr         = GCPtr;
    pNew->GCPtrLast     = GCPtrLast;
    pNew->cb            = cb;
    pNew->pszDesc       = pszDesc;
    pNew->pfnRelocate   = pfnRelocate;
    pNew->pvUser        = pvUser;
    pNew->cPTs          = cPTs;

    /*
     * Allocate page tables and insert them into the page directories.
     * (One 32-bit PT and two PAE PTs.)
     */
    uint8_t *pbPTs;
    rc = MMHyperAlloc(pVM, PAGE_SIZE * 3 * cPTs, PAGE_SIZE, MM_TAG_PGM, (void **)&pbPTs);
    if (VBOX_FAILURE(rc))
    {
        MMHyperFree(pVM, pNew);
        return VERR_NO_MEMORY;
    }

    /*
     * Init the page tables and insert them into the page directories.
     */
    Log4(("PGMR3MapPT: GCPtr=%VGv cPTs=%u pbPTs=%p\n", GCPtr, cPTs, pbPTs));
    for (i = 0; i < cPTs; i++)
    {
        /*
         * 32-bit.
         */
        pNew->aPTs[i].pPTR3    = (PX86PT)pbPTs;
        pNew->aPTs[i].pPTGC    = MMHyperR3ToGC(pVM, pNew->aPTs[i].pPTR3);
        pNew->aPTs[i].pPTR0    = MMHyperR3ToR0(pVM, pNew->aPTs[i].pPTR3);
        pNew->aPTs[i].HCPhysPT = MMR3HyperHCVirt2HCPhys(pVM, pNew->aPTs[i].pPTR3);
        pbPTs += PAGE_SIZE;
        Log4(("PGMR3MapPT: i=%d: pPTHC=%p pPTGC=%p HCPhysPT=%RHp\n",
              i, pNew->aPTs[i].pPTR3, pNew->aPTs[i].pPTGC, pNew->aPTs[i].HCPhysPT));

        /*
         * PAE.
         */
        pNew->aPTs[i].HCPhysPaePT0 = MMR3HyperHCVirt2HCPhys(pVM, pbPTs);
        pNew->aPTs[i].HCPhysPaePT1 = MMR3HyperHCVirt2HCPhys(pVM, pbPTs + PAGE_SIZE);
        pNew->aPTs[i].paPaePTsR3 = (PX86PTPAE)pbPTs;
        pNew->aPTs[i].paPaePTsGC = MMHyperR3ToGC(pVM, pbPTs);
        pNew->aPTs[i].paPaePTsR0 = MMHyperR3ToR0(pVM, pbPTs);
        pbPTs += PAGE_SIZE * 2;
        Log4(("PGMR3MapPT: i=%d: paPaePTsHC=%p paPaePTsGC=%p HCPhysPaePT0=%RHp HCPhysPaePT1=%RHp\n",
              i, pNew->aPTs[i].paPaePTsR3, pNew->aPTs[i].paPaePTsGC, pNew->aPTs[i].HCPhysPaePT0, pNew->aPTs[i].HCPhysPaePT1));
    }
    pgmR3MapSetPDEs(pVM, pNew, iPageDir);

    /*
     * Insert the new mapping.
     */
    pNew->pNextR3 = pCur;
    pNew->pNextGC = pCur ? MMHyperR3ToGC(pVM, pCur) : 0;
    pNew->pNextR0 = pCur ? MMHyperR3ToR0(pVM, pCur) : 0;
    if (pPrev)
    {
        pPrev->pNextR3 = pNew;
        pPrev->pNextGC = MMHyperR3ToGC(pVM, pNew);
        pPrev->pNextR0 = MMHyperR3ToR0(pVM, pNew);
    }
    else
    {
        pVM->pgm.s.pMappingsR3 = pNew;
        pVM->pgm.s.pMappingsGC = MMHyperR3ToGC(pVM, pNew);
        pVM->pgm.s.pMappingsR0 = MMHyperR3ToR0(pVM, pNew);
    }

    VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
    return VINF_SUCCESS;
}


/**
 * Removes a page table based mapping.
 *
 * @returns VBox status code.
 * @param   pVM     VM Handle.
 * @param   GCPtr   Virtual Address. (Page table aligned!)
 */
PGMR3DECL(int)  PGMR3UnmapPT(PVM pVM, RTGCPTR GCPtr)
{
    LogFlow(("PGMR3UnmapPT: GCPtr=%#x\n", GCPtr));

    /*
     * Find it.
     */
    PPGMMAPPING pPrev = NULL;
    PPGMMAPPING pCur = pVM->pgm.s.pMappingsR3;
    while (pCur)
    {
        if (pCur->GCPtr == GCPtr)
        {
            /*
             * Unlink it.
             */
            if (pPrev)
            {
                pPrev->pNextR3 = pCur->pNextR3;
                pPrev->pNextGC = pCur->pNextGC;
                pPrev->pNextR0 = pCur->pNextR0;
            }
            else
            {
                pVM->pgm.s.pMappingsR3 = pCur->pNextR3;
                pVM->pgm.s.pMappingsGC = pCur->pNextGC;
                pVM->pgm.s.pMappingsR0 = pCur->pNextR0;
            }

            /*
             * Free the page table memory, clear page directory entries
             * and free the page tables and node memory.
             */
            MMHyperFree(pVM, pCur->aPTs[0].pPTR3);
            pgmR3MapClearPDEs(&pVM->pgm.s, pCur, pCur->GCPtr >> X86_PD_SHIFT);
            MMHyperFree(pVM, pCur);

            VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
            return VINF_SUCCESS;
        }

        /* done? */
        if (pCur->GCPtr > GCPtr)
            break;

        /* next */
        pPrev = pCur;
        pCur = pCur->pNextR3;
    }

    AssertMsgFailed(("No mapping for %#x found!\n", GCPtr));
    return VERR_INVALID_PARAMETER;
}


/**
 * Gets the size of the current guest mappings if they were to be
 * put next to oneanother.
 *
 * @returns VBox status code.
 * @param   pVM     The VM.
 * @param   pcb     Where to store the size.
 */
PGMR3DECL(int) PGMR3MappingsSize(PVM pVM, uint32_t *pcb)
{
    RTGCUINTPTR cb = 0;
    for (PPGMMAPPING pCur = pVM->pgm.s.pMappingsR3; pCur; pCur = pCur->pNextR3)
        cb += pCur->cb;

    *pcb = cb;
    AssertReturn(*pcb == cb, VERR_NUMBER_TOO_BIG);
    Log(("PGMR3MappingsSize: return %d (%#x) bytes\n", cb, cb));
    return VINF_SUCCESS;
}


/**
 * Fixes the guest context mappings in a range reserved from the Guest OS.
 *
 * @returns VBox status code.
 * @param   pVM         The VM.
 * @param   GCPtrBase   The address of the reserved range of guest memory.
 * @param   cb          The size of the range starting at GCPtrBase.
 */
PGMR3DECL(int) PGMR3MappingsFix(PVM pVM, RTGCPTR GCPtrBase, uint32_t cb)
{
    Log(("PGMR3MappingsFix: GCPtrBase=%#x cb=%#x\n", GCPtrBase, cb));

    /*
     * This is all or nothing at all. So, a tiny bit of paranoia first.
     */
    if (GCPtrBase & X86_PAGE_4M_OFFSET_MASK)
    {
        AssertMsgFailed(("GCPtrBase (%#x) has to be aligned on a 4MB address!\n", GCPtrBase));
        return VERR_INVALID_PARAMETER;
    }
    if (!cb || (cb & X86_PAGE_4M_OFFSET_MASK))
    {
        AssertMsgFailed(("cb (%#x) is 0 or not aligned on a 4MB address!\n", cb));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Before we do anything we'll do a forced PD sync to try make sure any
     * pending relocations because of these mappings have been resolved.
     */
    PGMSyncCR3(pVM, CPUMGetGuestCR0(pVM), CPUMGetGuestCR3(pVM), CPUMGetGuestCR4(pVM), true);

    /*
     * Check that it's not conflicting with a core code mapping in the intermediate page table.
     */
    unsigned    iPDNew = GCPtrBase >> X86_PD_SHIFT;
    unsigned    i = cb >> X86_PD_SHIFT;
    while (i-- > 0)
    {
        if (pVM->pgm.s.pInterPD->a[iPDNew + i].n.u1Present)
        {
            /* Check that it's not one or our mappings. */
            PPGMMAPPING pCur = pVM->pgm.s.pMappingsR3;
            while (pCur)
            {
                if (iPDNew + i - (pCur->GCPtr >> X86_PD_SHIFT) < (pCur->cb >> X86_PD_SHIFT))
                    break;
                pCur = pCur->pNextR3;
            }
            if (!pCur)
            {
                LogRel(("PGMR3MappingsFix: Conflicts with intermediate PDE %#x (GCPtrBase=%VGv cb=%#zx). The guest should retry.\n",
                        iPDNew + i, GCPtrBase, cb));
                return VERR_PGM_MAPPINGS_FIX_CONFLICT;
            }
        }
    }

    /*
     * Loop the mappings and check that they all agree on their new locations.
     */
    RTGCPTR     GCPtrCur = GCPtrBase;
    PPGMMAPPING pCur = pVM->pgm.s.pMappingsR3;
    while (pCur)
    {
        if (!pCur->pfnRelocate(pVM, pCur->GCPtr, GCPtrCur, PGMRELOCATECALL_SUGGEST, pCur->pvUser))
        {
            AssertMsgFailed(("The suggested fixed address %#x was rejected by '%s'!\n", GCPtrCur, pCur->pszDesc));
            return VERR_PGM_MAPPINGS_FIX_REJECTED;
        }
        /* next */
        GCPtrCur += pCur->cb;
        pCur = pCur->pNextR3;
    }
    if (GCPtrCur > GCPtrBase + cb)
    {
        AssertMsgFailed(("cb (%#x) is less than the required range %#x!\n", cb, GCPtrCur - GCPtrBase));
        return VERR_PGM_MAPPINGS_FIX_TOO_SMALL;
    }

    /*
     * Loop the table assigning the mappings to the passed in memory
     * and call their relocator callback.
     */
    GCPtrCur = GCPtrBase;
    pCur = pVM->pgm.s.pMappingsR3;
    while (pCur)
    {
        unsigned iPDOld = pCur->GCPtr >> X86_PD_SHIFT;
        iPDNew = GCPtrCur >> X86_PD_SHIFT;

        /*
         * Relocate the page table(s).
         */
        pgmR3MapClearPDEs(&pVM->pgm.s, pCur, iPDOld);
        pgmR3MapSetPDEs(pVM, pCur, iPDNew);

        /*
         * Update the entry.
         */
        pCur->GCPtr = GCPtrCur;
        pCur->GCPtrLast = GCPtrCur + pCur->cb - 1;

        /*
         * Callback to execute the relocation.
         */
        pCur->pfnRelocate(pVM, iPDOld << X86_PD_SHIFT, iPDNew << X86_PD_SHIFT, PGMRELOCATECALL_RELOCATE, pCur->pvUser);

        /*
         * Advance.
         */
        GCPtrCur += pCur->cb;
        pCur = pCur->pNextR3;
    }

    /*
     * Turn off CR3 updating monitoring.
     */
    int rc2 = PGM_GST_PFN(UnmonitorCR3, pVM)(pVM);
    AssertRC(rc2);

    /*
     * Mark the mappings as fixed and return.
     */
    pVM->pgm.s.fMappingsFixed    = true;
    pVM->pgm.s.GCPtrMappingFixed = GCPtrBase;
    pVM->pgm.s.cbMappingFixed    = cb;
    pVM->pgm.s.fSyncFlags       &= ~PGM_SYNC_MONITOR_CR3;
    VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
    return VINF_SUCCESS;
}


/**
 * Unfixes the mappings.
 * After calling this function mapping conflict detection will be enabled.
 *
 * @returns VBox status code.
 * @param   pVM         The VM.
 */
PGMR3DECL(int) PGMR3MappingsUnfix(PVM pVM)
{
    Log(("PGMR3MappingsUnfix: fMappingsFixed=%d\n", pVM->pgm.s.fMappingsFixed));
    pVM->pgm.s.fMappingsFixed    = false;
    pVM->pgm.s.GCPtrMappingFixed = 0;
    pVM->pgm.s.cbMappingFixed    = 0;
    VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);

    /*
     * Re-enable the CR3 monitoring.
     *
     * Paranoia: We flush the page pool before doing that because Windows
     * is using the CR3 page both as a PD and a PT, e.g. the pool may
     * be monitoring it.
     */
#ifdef PGMPOOL_WITH_MONITORING
    pgmPoolFlushAll(pVM);
#endif
    int rc = PGM_GST_PFN(MonitorCR3, pVM)(pVM, pVM->pgm.s.GCPhysCR3);
    AssertRC(rc);

    return VINF_SUCCESS;
}


/**
 * Map pages into the intermediate context (switcher code).
 * These pages are mapped at both the give virtual address and at
 * the physical address (for identity mapping).
 *
 * @returns VBox status code.
 * @param   pVM         The virtual machine.
 * @param   Addr        Intermediate context address of the mapping.
 * @param   HCPhys      Start of the range of physical pages. This must be entriely below 4GB!
 * @param   cbPages     Number of bytes to map.
 *
 * @remark  This API shall not be used to anything but mapping the switcher code.
 */
PGMR3DECL(int) PGMR3MapIntermediate(PVM pVM, RTUINTPTR Addr, RTHCPHYS HCPhys, unsigned cbPages)
{
    LogFlow(("PGMR3MapIntermediate: Addr=%RTptr HCPhys=%VHp cbPages=%#x\n", Addr, HCPhys, cbPages));

    /*
     * Adjust input.
     */
    cbPages += (uint32_t)HCPhys & PAGE_OFFSET_MASK;
    cbPages  = RT_ALIGN(cbPages, PAGE_SIZE);
    HCPhys  &= X86_PTE_PAE_PG_MASK;
    Addr    &= PAGE_BASE_MASK;
    /* We only care about the first 4GB, because on AMD64 we'll be repeating them all over the address space. */
    uint32_t uAddress = (uint32_t)Addr;

    /*
     * Assert input and state.
     */
    AssertMsg(pVM->pgm.s.offVM, ("Bad init order\n"));
    AssertMsg(pVM->pgm.s.pInterPD, ("Bad init order, paging.\n"));
    AssertMsg(cbPages <= (512 << PAGE_SHIFT), ("The mapping is too big %d bytes\n", cbPages));
    AssertMsg(HCPhys < _4G && HCPhys + cbPages < _4G, ("Addr=%RTptr HCPhys=%VHp cbPages=%d\n", Addr, HCPhys, cbPages));

    /*
     * Check for internal conflicts between the virtual address and the physical address.
     */
    if (    uAddress != HCPhys
        &&  (   uAddress < HCPhys
                ? HCPhys - uAddress < cbPages
                : uAddress - HCPhys < cbPages
            )
       )
    {
        AssertMsgFailed(("Addr=%RTptr HCPhys=%VHp cbPages=%d\n", Addr, HCPhys, cbPages));
        LogRel(("Addr=%RTptr HCPhys=%VHp cbPages=%d\n", Addr, HCPhys, cbPages));
        return VERR_PGM_MAPPINGS_FIX_CONFLICT; /** @todo new error code */
    }

    /* The intermediate mapping must not conflict with our default hypervisor address. */
    size_t  cbHyper;
    RTGCPTR pvHyperGC = MMHyperGetArea(pVM, &cbHyper);
    if (uAddress < pvHyperGC
        ? uAddress + cbPages > pvHyperGC
        : pvHyperGC + cbHyper > uAddress
       )
    {
        AssertMsgFailed(("Addr=%RTptr HyperGC=%VGv cbPages=%zu\n", Addr, pvHyperGC, cbPages));
        LogRel(("Addr=%RTptr HyperGC=%VGv cbPages=%zu\n", Addr, pvHyperGC, cbPages));
        return VERR_PGM_MAPPINGS_FIX_CONFLICT; /** @todo new error code */
    }

    const unsigned cPages = cbPages >> PAGE_SHIFT;
    int rc = pgmR3MapIntermediateCheckOne(pVM, uAddress, cPages, pVM->pgm.s.apInterPTs[0], pVM->pgm.s.apInterPaePTs[0]);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pgmR3MapIntermediateCheckOne(pVM, (uintptr_t)HCPhys, cPages, pVM->pgm.s.apInterPTs[1], pVM->pgm.s.apInterPaePTs[1]);
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Everythings fine, do the mapping.
     */
    pgmR3MapIntermediateDoOne(pVM, uAddress, HCPhys, cPages, pVM->pgm.s.apInterPTs[0], pVM->pgm.s.apInterPaePTs[0]);
    pgmR3MapIntermediateDoOne(pVM, (uintptr_t)HCPhys, HCPhys, cPages, pVM->pgm.s.apInterPTs[1], pVM->pgm.s.apInterPaePTs[1]);

    return VINF_SUCCESS;
}


/**
 * Validates that there are no conflicts for this mapping into the intermediate context.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   uAddress    Address of the mapping.
 * @param   cPages      Number of pages.
 * @param   pPTDefault      Pointer to the default page table for this mapping.
 * @param   pPTPaeDefault   Pointer to the default page table for this mapping.
 */
static int pgmR3MapIntermediateCheckOne(PVM pVM, uintptr_t uAddress, unsigned cPages, PX86PT pPTDefault, PX86PTPAE pPTPaeDefault)
{
    AssertMsg((uAddress >> X86_PD_SHIFT) + cPages <= 1024, ("64-bit fixme\n"));

    /*
     * Check that the ranges are available.
     * (This codes doesn't have to be fast.)
     */
    while (cPages > 0)
    {
        /*
         * 32-Bit.
         */
        unsigned iPDE = (uAddress >> X86_PD_SHIFT) & X86_PD_MASK;
        unsigned iPTE = (uAddress >> X86_PT_SHIFT) & X86_PT_MASK;
        PX86PT pPT = pPTDefault;
        if (pVM->pgm.s.pInterPD->a[iPDE].u)
        {
            RTHCPHYS HCPhysPT = pVM->pgm.s.pInterPD->a[iPDE].u & X86_PDE_PG_MASK;
            if (HCPhysPT == MMPage2Phys(pVM, pVM->pgm.s.apInterPTs[0]))
                pPT = pVM->pgm.s.apInterPTs[0];
            else if (HCPhysPT == MMPage2Phys(pVM, pVM->pgm.s.apInterPTs[1]))
                pPT = pVM->pgm.s.apInterPTs[1];
            else
            {
                /** @todo this must be handled with a relocation of the conflicting mapping!
                 * Which of course cannot be done because we're in the middle of the initialization. bad design! */
                AssertMsgFailed(("Conflict between core code and PGMR3Mapping(). uAddress=%VHv\n", uAddress));
                LogRel(("Conflict between core code and PGMR3Mapping(). uAddress=%VHv\n", uAddress));
                return VERR_PGM_MAPPINGS_FIX_CONFLICT; /** @todo error codes! */
            }
        }
        if (pPT->a[iPTE].u)
        {
            AssertMsgFailed(("Conflict iPTE=%#x iPDE=%#x uAddress=%VHv pPT->a[iPTE].u=%RX32\n", iPTE, iPDE, uAddress, pPT->a[iPTE].u));
            LogRel(("Conflict iPTE=%#x iPDE=%#x uAddress=%VHv pPT->a[iPTE].u=%RX32\n",
                    iPTE, iPDE, uAddress, pPT->a[iPTE].u));
            return VERR_PGM_MAPPINGS_FIX_CONFLICT; /** @todo error codes! */
        }

        /*
         * PAE.
         */
        const unsigned iPDPE= (uAddress >> X86_PDPT_SHIFT) & X86_PDPT_MASK;
        iPDE = (uAddress >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK;
        iPTE = (uAddress >> X86_PT_PAE_SHIFT) & X86_PT_PAE_MASK;
        Assert(iPDPE < 4);
        Assert(pVM->pgm.s.apInterPaePDs[iPDPE]);
        PX86PTPAE pPTPae = pPTPaeDefault;
        if (pVM->pgm.s.apInterPaePDs[iPDPE]->a[iPDE].u)
        {
            RTHCPHYS HCPhysPT = pVM->pgm.s.apInterPaePDs[iPDPE]->a[iPDE].u & X86_PDE_PAE_PG_MASK;
            if (HCPhysPT == MMPage2Phys(pVM, pVM->pgm.s.apInterPaePTs[0]))
                pPTPae = pVM->pgm.s.apInterPaePTs[0];
            else if (HCPhysPT == MMPage2Phys(pVM, pVM->pgm.s.apInterPaePTs[0]))
                pPTPae = pVM->pgm.s.apInterPaePTs[1];
            else
            {
                /** @todo this must be handled with a relocation of the conflicting mapping!
                 * Which of course cannot be done because we're in the middle of the initialization. bad design! */
                AssertMsgFailed(("Conflict between core code and PGMR3Mapping(). uAddress=%VHv\n", uAddress));
                LogRel(("Conflict between core code and PGMR3Mapping(). uAddress=%VHv\n", uAddress));
                return VERR_PGM_MAPPINGS_FIX_CONFLICT; /** @todo error codes! */
            }
        }
        if (pPTPae->a[iPTE].u)
        {
            AssertMsgFailed(("Conflict iPTE=%#x iPDE=%#x uAddress=%VHv pPTPae->a[iPTE].u=%#RX64\n", iPTE, iPDE, uAddress, pPTPae->a[iPTE].u));
            LogRel(("Conflict iPTE=%#x iPDE=%#x uAddress=%VHv pPTPae->a[iPTE].u=%#RX64\n",
                    iPTE, iPDE, uAddress, pPTPae->a[iPTE].u));
            return VERR_PGM_MAPPINGS_FIX_CONFLICT; /** @todo error codes! */
        }

        /* next */
        uAddress += PAGE_SIZE;
        cPages--;
    }

    return VINF_SUCCESS;
}



/**
 * Sets up the intermediate page tables for a verified mapping.
 *
 * @param   pVM             VM handle.
 * @param   uAddress        Address of the mapping.
 * @param   HCPhys          The physical address of the page range.
 * @param   cPages          Number of pages.
 * @param   pPTDefault      Pointer to the default page table for this mapping.
 * @param   pPTPaeDefault   Pointer to the default page table for this mapping.
 */
static void pgmR3MapIntermediateDoOne(PVM pVM, uintptr_t uAddress, RTHCPHYS HCPhys, unsigned cPages, PX86PT pPTDefault, PX86PTPAE pPTPaeDefault)
{
    while (cPages > 0)
    {
        /*
         * 32-Bit.
         */
        unsigned iPDE = (uAddress >> X86_PD_SHIFT) & X86_PD_MASK;
        unsigned iPTE = (uAddress >> X86_PT_SHIFT) & X86_PT_MASK;
        PX86PT pPT;
        if (pVM->pgm.s.pInterPD->a[iPDE].u)
            pPT = (PX86PT)MMPagePhys2Page(pVM, pVM->pgm.s.pInterPD->a[iPDE].u & X86_PDE_PG_MASK);
        else
        {
            pVM->pgm.s.pInterPD->a[iPDE].u = X86_PDE_P | X86_PDE_A | X86_PDE_RW
                                           | (uint32_t)MMPage2Phys(pVM, pPTDefault);
            pPT = pPTDefault;
        }
        pPT->a[iPTE].u = X86_PTE_P | X86_PTE_RW | X86_PTE_A | X86_PTE_D | (uint32_t)HCPhys;

        /*
         * PAE
         */
        const unsigned iPDPE= (uAddress >> X86_PDPT_SHIFT) & X86_PDPT_MASK;
        iPDE = (uAddress >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK;
        iPTE = (uAddress >> X86_PT_PAE_SHIFT) & X86_PT_PAE_MASK;
        Assert(iPDPE < 4);
        Assert(pVM->pgm.s.apInterPaePDs[iPDPE]);
        PX86PTPAE pPTPae;
        if (pVM->pgm.s.apInterPaePDs[iPDPE]->a[iPDE].u)
            pPTPae = (PX86PTPAE)MMPagePhys2Page(pVM, pVM->pgm.s.apInterPaePDs[iPDPE]->a[iPDE].u & X86_PDE_PAE_PG_MASK);
        else
        {
            pPTPae = pPTPaeDefault;
            pVM->pgm.s.apInterPaePDs[iPDPE]->a[iPDE].u = X86_PDE_P | X86_PDE_A | X86_PDE_RW
                                                       | MMPage2Phys(pVM, pPTPaeDefault);
        }
        pPTPae->a[iPTE].u = X86_PTE_P | X86_PTE_RW | X86_PTE_A | X86_PTE_D | HCPhys;

        /* next */
        cPages--;
        HCPhys += PAGE_SIZE;
        uAddress += PAGE_SIZE;
    }
}


/**
 * Clears all PDEs involved with the mapping.
 *
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   pMap        Pointer to the mapping in question.
 * @param   iOldPDE     The index of the 32-bit PDE corresponding to the base of the mapping.
 */
static void pgmR3MapClearPDEs(PPGM pPGM, PPGMMAPPING pMap, int iOldPDE)
{
    unsigned i = pMap->cPTs;
    iOldPDE += i;
    while (i-- > 0)
    {
        iOldPDE--;

        /*
         * 32-bit.
         */
        pPGM->pInterPD->a[iOldPDE].u   = 0;
        pPGM->pHC32BitPD->a[iOldPDE].u = 0;

        /*
         * PAE.
         */
        const int iPD = iOldPDE / 256;
        int iPDE = iOldPDE * 2 % 512;
        pPGM->apInterPaePDs[iPD]->a[iPDE].u = 0;
        pPGM->apHCPaePDs[iPD]->a[iPDE].u    = 0;
        iPDE++;
        pPGM->apInterPaePDs[iPD]->a[iPDE].u = 0;
        pPGM->apHCPaePDs[iPD]->a[iPDE].u    = 0;
    }
}


/**
 * Sets all PDEs involved with the mapping.
 *
 * @param   pVM         The VM handle.
 * @param   pMap        Pointer to the mapping in question.
 * @param   iNewPDE     The index of the 32-bit PDE corresponding to the base of the mapping.
 */
static void pgmR3MapSetPDEs(PVM pVM, PPGMMAPPING pMap, int iNewPDE)
{
    PPGM pPGM = &pVM->pgm.s;

    /* If mappings are not supposed to be put in the shadow page table, then this function is a nop. */
    if (!pgmMapAreMappingsEnabled(&pVM->pgm.s))
        return;

    Assert(PGMGetGuestMode(pVM) <= PGMMODE_32_BIT);

    /*
     * Init the page tables and insert them into the page directories.
     */
    unsigned i = pMap->cPTs;
    iNewPDE += i;
    while (i-- > 0)
    {
        iNewPDE--;

        /*
         * 32-bit.
         */
        if (pPGM->pHC32BitPD->a[iNewPDE].n.u1Present)
            pgmPoolFree(pVM, pPGM->pHC32BitPD->a[iNewPDE].u & X86_PDE_PG_MASK, PGMPOOL_IDX_PD, iNewPDE);
        X86PDE Pde;
        /* Default mapping page directory flags are read/write and supervisor; individual page attributes determine the final flags */
        Pde.u = PGM_PDFLAGS_MAPPING | X86_PDE_P | X86_PDE_A | X86_PDE_RW | X86_PDE_US | (uint32_t)pMap->aPTs[i].HCPhysPT;
        pPGM->pInterPD->a[iNewPDE]   = Pde;
        pPGM->pHC32BitPD->a[iNewPDE] = Pde;

        /*
         * PAE.
         */
        const int iPD = iNewPDE / 256;
        int iPDE = iNewPDE * 2 % 512;
        if (pPGM->apHCPaePDs[iPD]->a[iPDE].n.u1Present)
            pgmPoolFree(pVM, pPGM->apHCPaePDs[iPD]->a[iPDE].u & X86_PDE_PAE_PG_MASK, PGMPOOL_IDX_PAE_PD, iNewPDE * 2);
        X86PDEPAE PdePae0;
        PdePae0.u = PGM_PDFLAGS_MAPPING | X86_PDE_P | X86_PDE_A | X86_PDE_RW | X86_PDE_US | pMap->aPTs[i].HCPhysPaePT0;
        pPGM->apInterPaePDs[iPD]->a[iPDE] = PdePae0;
        pPGM->apHCPaePDs[iPD]->a[iPDE]    = PdePae0;

        iPDE++;
        if (pPGM->apHCPaePDs[iPD]->a[iPDE].n.u1Present)
            pgmPoolFree(pVM, pPGM->apHCPaePDs[iPD]->a[iPDE].u & X86_PDE_PAE_PG_MASK, PGMPOOL_IDX_PAE_PD, iNewPDE * 2 + 1);
        X86PDEPAE PdePae1;
        PdePae1.u = PGM_PDFLAGS_MAPPING | X86_PDE_P | X86_PDE_A | X86_PDE_RW | X86_PDE_US | pMap->aPTs[i].HCPhysPaePT1;
        pPGM->apInterPaePDs[iPD]->a[iPDE] = PdePae1;
        pPGM->apHCPaePDs[iPD]->a[iPDE]    = PdePae1;
    }
}

/**
 * Relocates a mapping to a new address.
 *
 * @param   pVM         VM handle.
 * @param   pMapping    The mapping to relocate.
 * @param   iPDOld      Old page directory index.
 * @param   iPDNew      New page directory index.
 */
void pgmR3MapRelocate(PVM pVM, PPGMMAPPING pMapping, int iPDOld, int iPDNew)
{
    Log(("PGM: Relocating %s from %#x to %#x\n", pMapping->pszDesc, iPDOld << X86_PD_SHIFT, iPDNew << X86_PD_SHIFT));
    Assert(((unsigned)iPDOld << X86_PD_SHIFT) == pMapping->GCPtr);

    /*
     * Relocate the page table(s).
     */
    pgmR3MapClearPDEs(&pVM->pgm.s, pMapping, iPDOld);
    pgmR3MapSetPDEs(pVM, pMapping, iPDNew);

    /*
     * Update and resort the mapping list.
     */

    /* Find previous mapping for pMapping, put result into pPrevMap. */
    PPGMMAPPING pPrevMap = NULL;
    PPGMMAPPING pCur = pVM->pgm.s.pMappingsR3;
    while (pCur && pCur != pMapping)
    {
        /* next */
        pPrevMap = pCur;
        pCur = pCur->pNextR3;
    }
    Assert(pCur);

    /* Find mapping which >= than pMapping. */
    RTGCPTR     GCPtrNew = iPDNew << X86_PD_SHIFT;
    PPGMMAPPING pPrev = NULL;
    pCur = pVM->pgm.s.pMappingsR3;
    while (pCur && pCur->GCPtr < GCPtrNew)
    {
        /* next */
        pPrev = pCur;
        pCur = pCur->pNextR3;
    }

    if (pCur != pMapping && pPrev != pMapping)
    {
        /*
         * Unlink.
         */
        if (pPrevMap)
        {
            pPrevMap->pNextR3 = pMapping->pNextR3;
            pPrevMap->pNextGC = pMapping->pNextGC;
            pPrevMap->pNextR0 = pMapping->pNextR0;
        }
        else
        {
            pVM->pgm.s.pMappingsR3 = pMapping->pNextR3;
            pVM->pgm.s.pMappingsGC = pMapping->pNextGC;
            pVM->pgm.s.pMappingsR0 = pMapping->pNextR0;
        }

        /*
         * Link
         */
        pMapping->pNextR3 = pCur;
        if (pPrev)
        {
            pMapping->pNextGC = pPrev->pNextGC;
            pMapping->pNextR0 = pPrev->pNextR0;
            pPrev->pNextR3 = pMapping;
            pPrev->pNextGC = MMHyperR3ToGC(pVM, pMapping);
            pPrev->pNextR0 = MMHyperR3ToR0(pVM, pMapping);
        }
        else
        {
            pMapping->pNextGC = pVM->pgm.s.pMappingsGC;
            pMapping->pNextR0 = pVM->pgm.s.pMappingsR0;
            pVM->pgm.s.pMappingsR3 = pMapping;
            pVM->pgm.s.pMappingsGC = MMHyperR3ToGC(pVM, pMapping);
            pVM->pgm.s.pMappingsR0 = MMHyperR3ToR0(pVM, pMapping);
        }
    }

    /*
     * Update the entry.
     */
    pMapping->GCPtr = GCPtrNew;
    pMapping->GCPtrLast = GCPtrNew + pMapping->cb - 1;

    /*
     * Callback to execute the relocation.
     */
    pMapping->pfnRelocate(pVM, iPDOld << X86_PD_SHIFT, iPDNew << X86_PD_SHIFT, PGMRELOCATECALL_RELOCATE, pMapping->pvUser);
}


/**
 * Resolves a conflict between a page table based GC mapping and
 * the Guest OS page tables. (32 bits version)
 *
 * @returns VBox status code.
 * @param   pVM         VM Handle.
 * @param   pMapping    The mapping which conflicts.
 * @param   pPDSrc      The page directory of the guest OS.
 * @param   iPDOld      The index to the start of the current mapping.
 */
int pgmR3SyncPTResolveConflict(PVM pVM, PPGMMAPPING pMapping, PX86PD pPDSrc, int iPDOld)
{
    STAM_PROFILE_START(&pVM->pgm.s.StatHCResolveConflict, a);

    /*
     * Scan for free page directory entries.
     *
     * Note that we do not support mappings at the very end of the
     * address space since that will break our GCPtrEnd assumptions.
     */
    const unsigned  cPTs = pMapping->cPTs;
    unsigned        iPDNew = ELEMENTS(pPDSrc->a) - cPTs; /* (+ 1 - 1) */
    while (iPDNew-- > 0)
    {
        if (pPDSrc->a[iPDNew].n.u1Present)
            continue;
        if (cPTs > 1)
        {
            bool fOk = true;
            for (unsigned i = 1; fOk && i < cPTs; i++)
                if (pPDSrc->a[iPDNew + i].n.u1Present)
                    fOk = false;
            if (!fOk)
                continue;
        }

        /*
         * Check that it's not conflicting with an intermediate page table mapping.
         */
        bool        fOk = true;
        unsigned    i   = cPTs;
        while (fOk && i-- > 0)
            fOk = !pVM->pgm.s.pInterPD->a[iPDNew + i].n.u1Present;
        if (!fOk)
            continue;
        /** @todo AMD64 should check the PAE directories and skip the 32bit stuff. */

        /*
         * Ask the mapping.
         */
        if (pMapping->pfnRelocate(pVM, iPDOld << X86_PD_SHIFT, iPDNew << X86_PD_SHIFT, PGMRELOCATECALL_SUGGEST, pMapping->pvUser))
        {
            pgmR3MapRelocate(pVM, pMapping, iPDOld, iPDNew);
            STAM_PROFILE_STOP(&pVM->pgm.s.StatHCResolveConflict, a);
            return VINF_SUCCESS;
        }
    }

    STAM_PROFILE_STOP(&pVM->pgm.s.StatHCResolveConflict, a);
    AssertMsgFailed(("Failed to relocate page table mapping '%s' from %#x! (cPTs=%d)\n", pMapping->pszDesc, iPDOld << X86_PD_SHIFT, cPTs));
    return VERR_PGM_NO_HYPERVISOR_ADDRESS;
}


/**
 * Checks guest PD for conflicts with VMM GC mappings.
 *
 * @returns true if conflict detected.
 * @returns false if not.
 * @param   pVM         The virtual machine.
 * @param   cr3         Guest context CR3 register.
 * @param   fRawR0      Whether RawR0 is enabled or not.
 */
PGMR3DECL(bool) PGMR3MapHasConflicts(PVM pVM, uint32_t cr3, bool fRawR0) /** @todo how many HasConflict constructs do we really need? */
{
    /*
     * Can skip this if mappings are safely fixed.
     */
    if (pVM->pgm.s.fMappingsFixed)
        return false;

    Assert(PGMGetGuestMode(pVM) <= PGMMODE_32_BIT);

    /*
     * Resolve the page directory.
     */
    PX86PD pPD = pVM->pgm.s.pGuestPDHC; /** @todo Fix PAE! */
    Assert(pPD);
    Assert(pPD == (PX86PD)MMPhysGCPhys2HCVirt(pVM, cr3 & X86_CR3_PAGE_MASK, sizeof(*pPD)));

    /*
     * Iterate mappings.
     */
    for (PPGMMAPPING pCur = pVM->pgm.s.pMappingsR3; pCur; pCur = pCur->pNextR3)
    {
        unsigned iPDE = pCur->GCPtr >> X86_PD_SHIFT;
        unsigned iPT = pCur->cPTs;
        while (iPT-- > 0)
            if (    pPD->a[iPDE + iPT].n.u1Present /** @todo PGMGstGetPDE. */
                &&  (fRawR0 || pPD->a[iPDE + iPT].n.u1User))
            {
                STAM_COUNTER_INC(&pVM->pgm.s.StatHCDetectedConflicts);
                #if 1
                Log(("PGMR3HasMappingConflicts: Conflict was detected at %VGv for mapping %s\n"
                     "                          iPDE=%#x iPT=%#x PDE=%VGp.\n",
                     (iPT + iPDE) << X86_PD_SHIFT, pCur->pszDesc,
                     iPDE, iPT, pPD->a[iPDE + iPT].au32[0]));
                #else
                AssertMsgFailed(("PGMR3HasMappingConflicts: Conflict was detected at %VGv for mapping %s\n"
                                 "                          iPDE=%#x iPT=%#x PDE=%VGp.\n",
                                 (iPT + iPDE) << X86_PD_SHIFT, pCur->pszDesc,
                                 iPDE, iPT, pPD->a[iPDE + iPT].au32[0]));
                #endif
                return true;
            }
    }

    return false;
}


/**
 * Read memory from the guest mappings.
 *
 * This will use the page tables associated with the mappings to
 * read the memory. This means that not all kind of memory is readable
 * since we don't necessarily know how to convert that physical address
 * to a HC virtual one.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   pvDst       The destination address (HC of course).
 * @param   GCPtrSrc    The source address (GC virtual address).
 * @param   cb          Number of bytes to read.
 */
PGMR3DECL(int) PGMR3MapRead(PVM pVM, void *pvDst, RTGCPTR GCPtrSrc, size_t cb)
{
/** @todo remove this simplicity hack */
    /*
     * Simplicity over speed... Chop the request up into chunks
     * which don't cross pages.
     */
    if (cb + (GCPtrSrc & PAGE_OFFSET_MASK) > PAGE_SIZE)
    {
        for (;;)
        {
            unsigned cbRead = RT_MIN(cb, PAGE_SIZE - (GCPtrSrc & PAGE_OFFSET_MASK));
            int rc = PGMR3MapRead(pVM, pvDst, GCPtrSrc, cbRead);
            if (VBOX_FAILURE(rc))
                return rc;
            cb -= cbRead;
            if (!cb)
                break;
            pvDst = (char *)pvDst + cbRead;
            GCPtrSrc += cbRead;
        }
        return VINF_SUCCESS;
    }

    /*
     * Find the mapping.
     */
    PPGMMAPPING pCur = CTXALLSUFF(pVM->pgm.s.pMappings);
    while (pCur)
    {
        RTGCUINTPTR off = (RTGCUINTPTR)GCPtrSrc - (RTGCUINTPTR)pCur->GCPtr;
        if (off < pCur->cb)
        {
            if (off + cb > pCur->cb)
            {
                AssertMsgFailed(("Invalid page range %VGv LB%#x. mapping '%s' %VGv to %VGv\n",
                                 GCPtrSrc, cb, pCur->pszDesc, pCur->GCPtr, pCur->GCPtrLast));
                return VERR_INVALID_PARAMETER;
            }

            unsigned iPT  = off >> X86_PD_SHIFT;
            unsigned iPTE = (off >> PAGE_SHIFT) & X86_PT_MASK;
            while (cb > 0 && iPTE < ELEMENTS(CTXALLSUFF(pCur->aPTs[iPT].pPT)->a))
            {
                if (!CTXALLSUFF(pCur->aPTs[iPT].paPaePTs)[iPTE / 512].a[iPTE % 512].n.u1Present)
                    return VERR_PAGE_NOT_PRESENT;
                RTHCPHYS HCPhys = CTXALLSUFF(pCur->aPTs[iPT].paPaePTs)[iPTE / 512].a[iPTE % 512].u & X86_PTE_PAE_PG_MASK;

                /*
                 * Get the virtual page from the physical one.
                 */
                void *pvPage;
                int rc = MMR3HCPhys2HCVirt(pVM, HCPhys, &pvPage);
                if (VBOX_FAILURE(rc))
                    return rc;

                memcpy(pvDst, (char *)pvPage + (GCPtrSrc & PAGE_OFFSET_MASK), cb);
                return VINF_SUCCESS;
            }
        }

        /* next */
        pCur = CTXALLSUFF(pCur->pNext);
    }

    return VERR_INVALID_POINTER;
}


/**
 * Info callback for 'pgmhandlers'.
 *
 * @param   pHlp        The output helpers.
 * @param   pszArgs     The arguments. phys or virt.
 */
DECLCALLBACK(void) pgmR3MapInfo(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    pHlp->pfnPrintf(pHlp, pVM->pgm.s.fMappingsFixed 
                    ? "\nThe mappings are FIXED.\n" 
                    : "\nThe mappings are FLOATING.\n");
    PPGMMAPPING pCur;
    for (pCur = pVM->pgm.s.pMappingsR3; pCur; pCur = pCur->pNextR3)
        pHlp->pfnPrintf(pHlp, "%VGv - %VGv  %s\n", pCur->GCPtr, pCur->GCPtrLast, pCur->pszDesc);
}

