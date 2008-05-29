/* $Id$ */
/** @file
 * CSAM - Guest OS Code Scanning and Analysis Manager - Any Context
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
#define LOG_GROUP LOG_GROUP_CSAM
#include <VBox/cpum.h>
#include <VBox/stam.h>
#include <VBox/patm.h>
#include <VBox/csam.h>
#include <VBox/pgm.h>
#include <VBox/mm.h>
#include <VBox/sup.h>
#include <VBox/mm.h>
#include <VBox/param.h>
#include <iprt/avl.h>
#include "CSAMInternal.h"
#include <VBox/vm.h>
#include <VBox/dbg.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <VBox/dis.h>
#include <VBox/disopcode.h>
#include <iprt/string.h>
#include <iprt/asm.h>

/**
 * Check if this page needs to be analysed by CSAM
 *
 * @returns VBox status code
 * @param   pVM         The VM to operate on.
 * @param   pvFault     Fault address
 */
CSAMDECL(int) CSAMExecFault(PVM pVM, RTGCPTR pvFault)
{
    if(!CSAMIsEnabled(pVM))
        return VINF_SUCCESS;

    LogFlow(("CSAMGCExecFault: for page %08X scanned=%d\n", pvFault, CSAMIsPageScanned(pVM, pvFault)));

    if(CSAMIsPageScanned(pVM, pvFault))
    {
        // Already checked!
        STAM_COUNTER_ADD(&pVM->csam.s.StatNrKnownPagesGC, 1);
        return VINF_SUCCESS;
    }

    STAM_COUNTER_ADD(&pVM->csam.s.StatNrTraps, 1);
    VM_FF_SET(pVM, VM_FF_CSAM_SCAN_PAGE);
    return VINF_CSAM_PENDING_ACTION;
}


/**
 * Check if this page was previously scanned by CSAM
 *
 * @returns true -> scanned, false -> not scanned
 * @param   pVM         The VM to operate on.
 * @param   pPage       GC page address
 */
CSAMDECL(bool) CSAMIsPageScanned(PVM pVM, RTGCPTR pPage)
{
    int pgdir, bit;
    uintptr_t page;

    page  = (uintptr_t)pPage;
    pgdir = page >> X86_PAGE_4M_SHIFT;
    bit   = (page & X86_PAGE_4M_OFFSET_MASK) >> X86_PAGE_4K_SHIFT;

    Assert(pgdir < CSAM_PGDIRBMP_CHUNKS);
    Assert(bit < PAGE_SIZE);

    return pVM->csam.s.CTXSUFF(pPDBitmap)[pgdir] && ASMBitTest((void *)pVM->csam.s.CTXSUFF(pPDBitmap)[pgdir], bit);
}



/**
 * Mark a page as scanned/not scanned
 *
 * @note: we always mark it as scanned, even if we haven't completely done so
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPage       GC page address (not necessarily aligned)
 * @param   fScanned    Mark as scanned or not scanned
 *
 */
CSAMDECL(int) CSAMMarkPage(PVM pVM, RTGCPTR pPage, bool fScanned)
{
    int pgdir, bit;
    uintptr_t page;

#ifdef LOG_ENABLED
    if (fScanned && !CSAMIsPageScanned(pVM, pPage))
       Log(("CSAMMarkPage %VGv\n", pPage));
#endif

    if(!CSAMIsEnabled(pVM))
        return VINF_SUCCESS;

    page  = (uintptr_t)pPage;
    pgdir = page >> X86_PAGE_4M_SHIFT;
    bit   = (page & X86_PAGE_4M_OFFSET_MASK) >> X86_PAGE_4K_SHIFT;

    Assert(pgdir < CSAM_PGDIRBMP_CHUNKS);
    Assert(bit < PAGE_SIZE);

    if(!CTXSUFF(pVM->csam.s.pPDBitmap)[pgdir])
    {
        STAM_COUNTER_INC(&pVM->csam.s.StatBitmapAlloc);
        int rc = MMHyperAlloc(pVM, CSAM_PAGE_BITMAP_SIZE, 0, MM_TAG_CSAM, (void **)&pVM->csam.s.CTXSUFF(pPDBitmap)[pgdir]);
        if (VBOX_FAILURE(rc))
        {
            Log(("MMR3HyperAlloc failed with %d\n", rc));
            return rc;
        }
#ifdef IN_GC
        pVM->csam.s.pPDHCBitmapGC[pgdir] = MMHyperGC2HC(pVM, (RCPTRTYPE(void*))pVM->csam.s.pPDBitmapGC[pgdir]);
        if (!pVM->csam.s.pPDHCBitmapGC[pgdir])
        {
            Log(("MMHyperHC2GC failed for %VGv\n", pVM->csam.s.pPDBitmapGC[pgdir]));
            return rc;
        }
#else
        pVM->csam.s.pPDGCBitmapHC[pgdir] = MMHyperHC2GC(pVM, pVM->csam.s.pPDBitmapHC[pgdir]);
        if (!pVM->csam.s.pPDGCBitmapHC[pgdir])
        {
            Log(("MMHyperHC2GC failed for %VHv\n", pVM->csam.s.pPDBitmapHC[pgdir]));
            return rc;
        }
#endif
    }
    if(fScanned)
        ASMBitSet((void *)pVM->csam.s.CTXSUFF(pPDBitmap)[pgdir], bit);
    else
        ASMBitClear((void *)pVM->csam.s.CTXSUFF(pPDBitmap)[pgdir], bit);

    return VINF_SUCCESS;
}

/**
 * Check if this page needs to be analysed by CSAM.
 *
 * This function should only be called for supervisor pages and
 * only when CSAM is enabled. Leaving these selection criteria
 * to the caller simplifies the interface (PTE passing).
 *
 * Note the the page has not yet been synced, so the TLB trick
 * (which wasn't ever active anyway) cannot be applied.
 *
 * @returns true if the page should be marked not present because
 *          CSAM want need to scan it.
 * @returns false if the page was already scanned.
 * @param   pVM         The VM to operate on.
 * @param   GCPtr       GC pointer of page
 */
CSAMDECL(bool) CSAMDoesPageNeedScanning(PVM pVM, RTGCPTR GCPtr)
{
    if(!CSAMIsEnabled(pVM))
        return false;

    if(CSAMIsPageScanned(pVM, GCPtr))
    {
        /* Already checked! */
        STAM_COUNTER_ADD(&CTXSUFF(pVM->csam.s.StatNrKnownPages), 1);
        return false;
    }
    STAM_COUNTER_ADD(&CTXSUFF(pVM->csam.s.StatNrPageNP), 1);
    return true;
}


/**
 * Remember a possible code page for later inspection
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   GCPtr       GC pointer of page
 */
CSAMDECL(void) CSAMMarkPossibleCodePage(PVM pVM, RTGCPTR GCPtr)
{
    if (pVM->csam.s.cPossibleCodePages < RT_ELEMENTS(pVM->csam.s.pvPossibleCodePage))
    {
        pVM->csam.s.pvPossibleCodePage[pVM->csam.s.cPossibleCodePages++] = (RTGCPTR32)GCPtr;
        VM_FF_SET(pVM, VM_FF_CSAM_PENDING_ACTION);
    }
    return;
}


/**
 * Turn on code scanning
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
CSAMDECL(int) CSAMEnableScanning(PVM pVM)
{
    pVM->fCSAMEnabled = true;
    return VINF_SUCCESS;
}

/**
 * Turn off code scanning
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
CSAMDECL(int) CSAMDisableScanning(PVM pVM)
{
    pVM->fCSAMEnabled = false;
    return VINF_SUCCESS;
}


/**
 * Check if we've scanned this instruction before. If true, then we can emulate
 * it instead of returning to ring 3.
 *
 * Using a simple array here as there are generally few mov crx instructions and
 * tree lookup is likely to be more expensive. (as it would also have to be offset based)
 *
 * @returns boolean
 * @param   pVM         The VM to operate on.
 * @param   GCPtr       GC pointer of page table entry
 */
CSAMDECL(bool) CSAMIsKnownDangerousInstr(PVM pVM, RTGCPTR GCPtr)
{
    for (uint32_t i=0;i<pVM->csam.s.cDangerousInstr;i++)
    {
        if (pVM->csam.s.aDangerousInstr[i] == (RTGCPTR32)GCPtr)
        {
            STAM_COUNTER_INC(&pVM->csam.s.StatInstrCacheHit);
            return true;
        }
    }
    /* Record that we're about to process it in ring 3. */
    pVM->csam.s.aDangerousInstr[pVM->csam.s.iDangerousInstr++] = (RTGCPTR32)GCPtr;
    pVM->csam.s.iDangerousInstr &= CSAM_MAX_DANGR_INSTR_MASK;

    if (++pVM->csam.s.cDangerousInstr > CSAM_MAX_DANGR_INSTR)
        pVM->csam.s.cDangerousInstr = CSAM_MAX_DANGR_INSTR;

    STAM_COUNTER_INC(&pVM->csam.s.StatInstrCacheMiss);
    return false;
}
