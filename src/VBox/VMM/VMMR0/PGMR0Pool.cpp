/* $Id$ */
/** @file
 * PGM Shadow Page Pool, ring-0 specific bits.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_PGM_POOL
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/hm.h>
#include "PGMInternal.h"
#include <VBox/vmm/vmcc.h>
#include "PGMInline.h"

#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/mem.h>
#include <iprt/memobj.h>



/**
 * Grows the shadow page pool.
 *
 * I.e. adds more pages to it, assuming that hasn't reached cMaxPages yet.
 *
 * @returns VBox status code.
 * @param   pGVM    The ring-0 VM structure.
 */
VMMR0_INT_DECL(int) PGMR0PoolGrow(PGVM pGVM)
{
    PPGMPOOL pPool = pGVM->pgm.s.pPoolR0;
    AssertReturn(pPool->cCurPages < pPool->cMaxPages, VERR_PGM_POOL_MAXED_OUT_ALREADY);
    AssertReturn(pPool->pVMR3 == pGVM->pVMR3, VERR_PGM_POOL_IPE);
    AssertReturn(pPool->pVMR0 == pGVM, VERR_PGM_POOL_IPE);

    /* With 32-bit guests and no EPT, the CR3 limits the root pages to low
       (below 4 GB) memory. */
    /** @todo change the pool to handle ROOT page allocations specially when
     *        required. */
    bool const fCanUseHighMemory = HMIsNestedPagingActive(pGVM);

    STAM_REL_PROFILE_START(&pPool->StatGrow, a);
    int rc = RTCritSectEnter(&pGVM->pgmr0.s.PoolGrowCritSect);
    AssertRCReturn(rc, rc);

    /*
     * Figure out how many pages should allocate.
     */
    uint32_t const cMaxPages = RT_MIN(pPool->cMaxPages, PGMPOOL_IDX_LAST);
    uint32_t const cCurPages = RT_MIN(pPool->cCurPages, cMaxPages);
    if (cCurPages < cMaxPages)
    {
        uint32_t cNewPages = cMaxPages - cCurPages;
        if (cNewPages > PGMPOOL_CFG_MAX_GROW)
            cNewPages = PGMPOOL_CFG_MAX_GROW;
        LogFlow(("PGMR3PoolGrow: Growing the pool by %u (%#x) pages to %u (%#x) pages. fCanUseHighMemory=%RTbool\n",
                 cNewPages, cNewPages, cCurPages + cNewPages, cCurPages + cNewPages, fCanUseHighMemory));

        /* Check that the handles in the arrays entry are both NIL. */
        uintptr_t const idxMemHandle = cCurPages / (PGMPOOL_CFG_MAX_GROW);
        AssertCompile(   (PGMPOOL_IDX_LAST + (PGMPOOL_CFG_MAX_GROW - 1)) / PGMPOOL_CFG_MAX_GROW
                      <= RT_ELEMENTS(pGVM->pgmr0.s.ahPoolMemObjs));
        AssertCompile(RT_ELEMENTS(pGVM->pgmr0.s.ahPoolMemObjs) == RT_ELEMENTS(pGVM->pgmr0.s.ahPoolMapObjs));
        AssertLogRelMsgReturnStmt(   pGVM->pgmr0.s.ahPoolMemObjs[idxMemHandle] == NIL_RTR0MEMOBJ
                                  && pGVM->pgmr0.s.ahPoolMapObjs[idxMemHandle] == NIL_RTR0MEMOBJ,
                                  ("idxMemHandle=%#x\n", idxMemHandle), RTCritSectLeave(&pGVM->pgmr0.s.PoolGrowCritSect),
                                  VERR_PGM_POOL_IPE);

        /*
         * Allocate the new pages and map them into ring-3.
         */
        RTR0MEMOBJ hMemObj = NIL_RTR0MEMOBJ;
        if (fCanUseHighMemory)
            rc = RTR0MemObjAllocPage(&hMemObj, cNewPages * PAGE_SIZE, false /*fExecutable*/);
        else
            rc = RTR0MemObjAllocLow(&hMemObj, cNewPages * PAGE_SIZE, false /*fExecutable*/);
        if (RT_SUCCESS(rc))
        {
            RTR0MEMOBJ hMapObj = NIL_RTR0MEMOBJ;
            rc = RTR0MemObjMapUser(&hMapObj, hMemObj, (RTR3PTR)-1, 0, RTMEM_PROT_READ | RTMEM_PROT_WRITE, NIL_RTR0PROCESS);
            if (RT_SUCCESS(rc))
            {
                pGVM->pgmr0.s.ahPoolMemObjs[idxMemHandle] = hMemObj;
                pGVM->pgmr0.s.ahPoolMapObjs[idxMemHandle] = hMapObj;

                uint8_t *pbRing0 = (uint8_t *)RTR0MemObjAddress(hMemObj);
                RTR3PTR  pbRing3 = RTR0MemObjAddressR3(hMapObj);
                AssertPtr(pbRing0);
                Assert(((uintptr_t)pbRing0 & PAGE_OFFSET_MASK) == 0);
                Assert(pbRing3 != NIL_RTR3PTR);
                Assert((pbRing3 & PAGE_OFFSET_MASK) == 0);

                /*
                 * Initialize the new pages.
                 */
                for (unsigned iNewPage = 0; iNewPage < cNewPages; iNewPage++)
                {
                    PPGMPOOLPAGE pPage = &pPool->aPages[cCurPages + iNewPage];
                    pPage->pvPageR0         = &pbRing0[iNewPage * PAGE_SIZE];
                    pPage->pvPageR3         = pbRing3 + iNewPage * PAGE_SIZE;
                    pPage->Core.Key         = RTR0MemObjGetPagePhysAddr(hMemObj, iNewPage);
                    AssertFatal(pPage->Core.Key < _4G || fCanUseHighMemory);
                    pPage->GCPhys           = NIL_RTGCPHYS;
                    pPage->enmKind          = PGMPOOLKIND_FREE;
                    pPage->idx              = pPage - &pPool->aPages[0];
                    LogFlow(("PGMR3PoolGrow: insert page #%#x - %RHp\n", pPage->idx, pPage->Core.Key));
                    pPage->iNext            = pPool->iFreeHead;
                    pPage->iUserHead        = NIL_PGMPOOL_USER_INDEX;
                    pPage->iModifiedNext    = NIL_PGMPOOL_IDX;
                    pPage->iModifiedPrev    = NIL_PGMPOOL_IDX;
                    pPage->iMonitoredNext   = NIL_PGMPOOL_IDX;
                    pPage->iMonitoredPrev   = NIL_PGMPOOL_IDX;
                    pPage->iAgeNext         = NIL_PGMPOOL_IDX;
                    pPage->iAgePrev         = NIL_PGMPOOL_IDX;
                    /* commit it */
                    bool fRc = RTAvloHCPhysInsert(&pPool->HCPhysTree, &pPage->Core); Assert(fRc); NOREF(fRc);
                    pPool->iFreeHead = cCurPages + iNewPage;
                    pPool->cCurPages = cCurPages + iNewPage + 1;
                }

                STAM_REL_PROFILE_STOP(&pPool->StatGrow, a);
                RTCritSectLeave(&pGVM->pgmr0.s.PoolGrowCritSect);
                return VINF_SUCCESS;
            }

            RTR0MemObjFree(hMemObj, true /*fFreeMappings*/);
        }
        if (cCurPages > 0)
            rc = -rc;
        else
            LogRel(("PGMR0PoolGrow: rc=%Rrc cNewPages=%#x cCurPages=%#x cMaxPages=%#x fCanUseHighMemory=%d\n",
                    rc, cNewPages, cCurPages, cMaxPages, fCanUseHighMemory));
    }
    RTCritSectLeave(&pGVM->pgmr0.s.PoolGrowCritSect);
    return rc;
}

