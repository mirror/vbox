/* $Id$ */
/** @file
 * PGM - Page Manager and Monitor, Page Sharing, Ring-0.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/gmm.h>
#include "PGMInternal.h"
#include <VBox/vmm/vm.h>
#include "PGMInline.h"
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
 * @param   pGVM        Pointer to the GVM instance data.
 * @param   idCpu       VCPU id
 * @param   pModule     Module description
 * @param   cRegions    Number of regions
 * @param   pRegions    Region array
 */
VMMR0DECL(int) PGMR0SharedModuleCheck(PVM pVM, PGVM pGVM, VMCPUID idCpu, PGMMSHAREDMODULE pModule,
                                      uint32_t cRegions, PGMMSHAREDREGIONDESC pRegions)
{
    int                rc = VINF_SUCCESS;
    GMMSHAREDPAGEDESC  PageDesc;
    bool               fFlushTLBs = false;
    PVMCPU             pVCpu = &pVM->aCpus[idCpu];

    Log(("PGMR0SharedModuleCheck: check %s %s base=%RGv size=%x\n", pModule->szName, pModule->szVersion, pModule->Core.Key, pModule->cbModule));

    PGM_LOCK_ASSERT_OWNER(pVM);     /* This cannot fail as we grab the lock in pgmR3SharedModuleRegRendezvous before calling into ring-0. */

    /* Check every region of the shared module. */
    for (unsigned idxRegion = 0; idxRegion < cRegions; idxRegion++)
    {
        Assert((pRegions[idxRegion].cbRegion & 0xfff) == 0);
        Assert((pRegions[idxRegion].GCRegionAddr & 0xfff) == 0);

        RTGCPTR  GCRegion = pRegions[idxRegion].GCRegionAddr;
        unsigned cbRegion = pRegions[idxRegion].cbRegion & ~0xfff;
        unsigned idxPage  = 0;

        while (cbRegion)
        {
            RTGCPHYS GCPhys;
            uint64_t fFlags;

            /** @todo inefficient to fetch each guest page like this... */
            rc = PGMGstGetPage(pVCpu, GCRegion, &fFlags, &GCPhys);
            if (    rc == VINF_SUCCESS
                &&  !(fFlags & X86_PTE_RW)) /* important as we make assumptions about this below! */
            {
                PPGMPAGE pPage = pgmPhysGetPage(pVM, GCPhys);
                Assert(!pPage || !PGM_PAGE_IS_BALLOONED(pPage));
                if (    pPage
                    &&  PGM_PAGE_GET_STATE(pPage) == PGM_PAGE_STATE_ALLOCATED)
                {
                    PageDesc.idPage = PGM_PAGE_GET_PAGEID(pPage);
                    PageDesc.HCPhys = PGM_PAGE_GET_HCPHYS(pPage);
                    PageDesc.GCPhys = GCPhys;

                    rc = GMMR0SharedModuleCheckPage(pGVM, pModule, idxRegion, idxPage, &PageDesc);
                    if (rc == VINF_SUCCESS)
                    {
                        /* Any change for this page? */
                        if (PageDesc.idPage != NIL_GMM_PAGEID)
                        {
                            Assert(PGM_PAGE_GET_STATE(pPage) == PGM_PAGE_STATE_ALLOCATED);

                            Log(("PGMR0SharedModuleCheck: shared page gc virt=%RGv phys %RGp host %RHp->%RHp\n", pRegions[idxRegion].GCRegionAddr + idxPage * PAGE_SIZE, PageDesc.GCPhys, PGM_PAGE_GET_HCPHYS(pPage), PageDesc.HCPhys));
                            if (PageDesc.HCPhys != PGM_PAGE_GET_HCPHYS(pPage))
                            {
                                bool fFlush = false;

                                /* Page was replaced by an existing shared version of it; clear all references first. */
                                rc = pgmPoolTrackUpdateGCPhys(pVM, PageDesc.GCPhys, pPage, true /* clear the entries */, &fFlush);
                                Assert(rc == VINF_SUCCESS || (VMCPU_FF_ISSET(pVCpu, VMCPU_FF_PGM_SYNC_CR3) && (pVCpu->pgm.s.fSyncFlags & PGM_SYNC_CLEAR_PGM_POOL)));
                                if (rc == VINF_SUCCESS)
                                    fFlushTLBs |= fFlush;

                                /* Update the physical address and page id now. */
                                PGM_PAGE_SET_HCPHYS(pVM, pPage, PageDesc.HCPhys);
                                PGM_PAGE_SET_PAGEID(pVM, pPage, PageDesc.idPage);

                                /* Invalidate page map TLB entry for this page too. */
                                pgmPhysInvalidatePageMapTLBEntry(pVM, PageDesc.GCPhys);
                                pVM->pgm.s.cReusedSharedPages++;
                            }
                            /* else nothing changed (== this page is now a shared page), so no need to flush anything. */

                            pVM->pgm.s.cSharedPages++;
                            pVM->pgm.s.cPrivatePages--;
                            PGM_PAGE_SET_STATE(pVM, pPage, PGM_PAGE_STATE_SHARED);
                        }
                    }
                    else
                        break;
                }
            }
            else
            {
                Assert(    rc == VINF_SUCCESS
                       ||  rc == VERR_PAGE_NOT_PRESENT
                       ||  rc == VERR_PAGE_MAP_LEVEL4_NOT_PRESENT
                       ||  rc == VERR_PAGE_DIRECTORY_PTR_NOT_PRESENT
                       ||  rc == VERR_PAGE_TABLE_NOT_PRESENT);
                rc = VINF_SUCCESS;  /* ignore error */
            }

            idxPage++;
            GCRegion += PAGE_SIZE;
            cbRegion -= PAGE_SIZE;
        }
    }

    if (fFlushTLBs)
        PGM_INVL_ALL_VCPU_TLBS(pVM);

    return rc;
}
#endif /* VBOX_WITH_PAGE_SHARING */

