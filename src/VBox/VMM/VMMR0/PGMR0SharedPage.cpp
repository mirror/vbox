/* $Id: PGMR0.cpp 61539 2010-05-12 15:11:09Z sandervl $ */
/** @file
 * PGM - Page Manager and Monitor, Ring-0.
 */

/*
 * Copyright (C) 2007 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_PGM_SHARED
#include <VBox/pgm.h>
#include <VBox/gmm.h>
#include "../PGMInternal.h"
#include <VBox/vm.h>
#include "../PGMInline.h"
#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/mem.h>


#ifdef VBOX_WITH_PAGE_SHARING
/**
 * Check a registered module for shared page changes
 *
 * @returns The following VBox status codes.
 *
 * @param   pVM         The VM handle.
 * @param   idCpu       VCPU id
 * @param   pModule     Module description
 * @param   pGVM        Pointer to the GVM instance data.
 */
VMMR0DECL(int) PGMR0SharedModuleCheckRegion(PVM pVM, VMCPUID idCpu, PGMMSHAREDMODULE pModule, PGVM pGVM)
{
    int                rc = VINF_SUCCESS;
    PGMMSHAREDPAGEDESC paPageDesc = NULL;
    uint32_t           cbPreviousRegion  = 0;
    bool               fFlushTLBs = false;
    PVMCPU             pVCpu = &pVM->aCpus[idCpu];

    Log(("PGMR0SharedModuleCheck: check %s %s base=%RGv size=%x\n", pModule->szName, pModule->szVersion, pModule->Core.Key, pModule->cbModule));

    pgmLock(pVM);

    /* Check every region of the shared module. */
    for (unsigned idxModule = 0; idxModule < pModule->cRegions; idxModule++)
    {
        Assert((pModule->aRegions[idxModule].cbRegion & 0xfff) == 0);
        Assert((pModule->aRegions[idxModule].GCRegionAddr & 0xfff) == 0);

        RTGCPTR  GCRegion  = pModule->aRegions[idxModule].GCRegionAddr;
        unsigned cbRegion = pModule->aRegions[idxModule].cbRegion & ~0xfff;
        unsigned idxPage = 0;
        bool     fValidChanges = false;

        if (cbPreviousRegion < cbRegion)
        {
            if (paPageDesc)
                RTMemFree(paPageDesc);

            paPageDesc = (PGMMSHAREDPAGEDESC)RTMemAlloc((cbRegion >> PAGE_SHIFT) * sizeof(*paPageDesc));
            if (!paPageDesc)
            {
                AssertFailed();
                rc = VERR_NO_MEMORY;
                goto end;
            }
            cbPreviousRegion  = cbRegion;
        }

        while (cbRegion)
        {
            RTGCPHYS GCPhys;
            uint64_t fFlags;

            rc = PGMGstGetPage(pVCpu, GCRegion, &fFlags, &GCPhys);
            if (    rc == VINF_SUCCESS
                &&  !(fFlags & X86_PTE_RW)) /* important as we make assumptions about this below! */
            {
                PPGMPAGE pPage = pgmPhysGetPage(&pVM->pgm.s, GCPhys);
                if (    pPage
                    &&  !PGM_PAGE_IS_SHARED(pPage))
                {
                    fValidChanges = true;
                    paPageDesc[idxPage].uHCPhysPageId = PGM_PAGE_GET_PAGEID(pPage);
                    paPageDesc[idxPage].HCPhys        = PGM_PAGE_GET_HCPHYS(pPage);
                    paPageDesc[idxPage].GCPhys        = GCPhys;
                }
                else
                    paPageDesc[idxPage].uHCPhysPageId = NIL_GMM_PAGEID;
            }
            else
                paPageDesc[idxPage].uHCPhysPageId = NIL_GMM_PAGEID;

            idxPage++;
            GCRegion += PAGE_SIZE;
            cbRegion -= PAGE_SIZE;
        }

        if (fValidChanges)
        {
            rc = GMMR0SharedModuleCheckRange(pGVM, pModule, idxModule, idxPage, paPageDesc);
            AssertRC(rc);
            if (RT_FAILURE(rc))
                break;
                
            for (unsigned i = 0; i < idxPage; i++)
            {
                /* Any change for this page? */
                if (paPageDesc[i].uHCPhysPageId != NIL_GMM_PAGEID)
                {
                    /** todo: maybe cache these to prevent the nth lookup. */
                    PPGMPAGE pPage = pgmPhysGetPage(&pVM->pgm.s, paPageDesc[i].GCPhys);
                    if (!pPage)
                    {
                        /* Should never happen. */
                        AssertFailed();
                        rc = VERR_PGM_PHYS_INVALID_PAGE_ID;
                        goto end;
                    }
                    Assert(!PGM_PAGE_IS_SHARED(pPage));

                    Log(("PGMR0SharedModuleCheck: shared page gc virt=%RGv phys %RGp host %RHp->%RHp\n", pModule->aRegions[idxModule].GCRegionAddr + i * PAGE_SIZE, paPageDesc[i].GCPhys, PGM_PAGE_GET_HCPHYS(pPage), paPageDesc[i].HCPhys));
                    if (paPageDesc[i].HCPhys != PGM_PAGE_GET_HCPHYS(pPage))
                    {
                        bool fFlush = false;

                        /* Page was replaced by an existing shared version of it; clear all references first. */
                        rc = pgmPoolTrackUpdateGCPhys(pVM, paPageDesc[i].GCPhys, pPage, true /* clear the entries */, &fFlush);
                        if (RT_FAILURE(rc))
                        {
                            AssertRC(rc);
                            goto end;
                        }
                        Assert(rc == VINF_SUCCESS || (VMCPU_FF_ISSET(pVCpu, VMCPU_FF_PGM_SYNC_CR3) && (pVCpu->pgm.s.fSyncFlags & PGM_SYNC_CLEAR_PGM_POOL)));
                        if (rc = VINF_SUCCESS)
                            fFlushTLBs |= fFlush;

                        /* Update the physical address and page id now. */
                        PGM_PAGE_SET_HCPHYS(pPage, paPageDesc[i].HCPhys);
                        PGM_PAGE_SET_PAGEID(pPage, paPageDesc[i].uHCPhysPageId);

                        /* Invalidate page map TLB entry for this page too. */
                        PGMPhysInvalidatePageMapTLBEntry(pVM, paPageDesc[i].GCPhys);
                    }
                    /* else nothing changed (== this page is now a shared page), so no need to flush anything. */

                    pVM->pgm.s.cSharedPages++;
                    pVM->pgm.s.cPrivatePages--;
                    PGM_PAGE_SET_STATE(pPage, PGM_PAGE_STATE_SHARED);
                }
            }
        }
        else
            rc = VINF_SUCCESS;  /* nothing to do. */
    }

end:
    pgmUnlock(pVM);
    if (fFlushTLBs)
        PGM_INVL_ALL_VCPU_TLBS(pVM);

    if (paPageDesc)
        RTMemFree(paPageDesc);

    return rc;
}
#endif

