/* $Id$ */
/** @file
 * GMM - Global Memory Manager, ring-3 request wrappers.
 */

/*
 * Copyright (C) 2008 innotek GmbH
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
#include <VBox/gmm.h>
#include <VBox/vmm.h>
#include <VBox/vm.h>
#include <VBox/sup.h>
#include <VBox/err.h>
#include <VBox/param.h>

#include <iprt/mem.h>


/**
 * @see GMMR0InitialReservation
 */
GMMR3DECL(int)  GMMR3InitialReservation(PVM pVM, uint64_t cBasePages, uint32_t cShadowPages, uint32_t cFixedPages,
                                        GMMOCPOLICY enmPolicy, GMMPRIORITY enmPriority)
{
    GMMINITIALRESERVATIONREQ Req;
    Req.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    Req.Hdr.cbReq = sizeof(Req);
    Req.cBasePages = cBasePages;
    Req.cShadowPages = cShadowPages;
    Req.cFixedPages = cFixedPages;
    Req.enmPolicy = enmPolicy;
    Req.enmPriority = enmPriority;
    return SUPCallVMMR0Ex(pVM->pVMR0, VMMR0_DO_GMM_INITIAL_RESERVATION, 0, &Req.Hdr);
}


/**
 * @see GMMR0UpdateReservation
 */
GMMR3DECL(int)  GMMR3UpdateReservation(PVM pVM, uint64_t cBasePages, uint32_t cShadowPages, uint32_t cFixedPages)
{
    GMMUPDATERESERVATIONREQ Req;
    Req.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    Req.Hdr.cbReq = sizeof(Req);
    Req.cBasePages = cBasePages;
    Req.cShadowPages = cShadowPages;
    Req.cFixedPages = cFixedPages;
    return SUPCallVMMR0Ex(pVM->pVMR0, VMMR0_DO_GMM_UPDATE_RESERVATION, 0, &Req.Hdr);
}


/**
 * Prepares a GMMR0AllocatePages request.
 */
GMMR3DECL(int) GMMR3AllocatePagesPrepare(PVM pVM, PGMMALLOCATEPAGESREQ *ppReq, uint32_t cPages, GMMACCOUNT enmAccount)
{
    uint32_t cb = RT_OFFSETOF(GMMALLOCATEPAGESREQ, aPages[cPages]);
    PGMMALLOCATEPAGESREQ pReq = (PGMMALLOCATEPAGESREQ)RTMemTmpAllocZ(cb);
    if (!pReq)
        return VERR_NO_TMP_MEMORY;

    pReq->Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    pReq->Hdr.cbReq = cb;
    pReq->enmAccount = enmAccount;
    pReq->cPages = cPages;
    NOREF(pVM);
    return VINF_SUCCESS;
}


/**
 * Performs a GMMR0AllocatePages request.
 * This will call VMSetError on failure.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the shared VM structure.
 * @param   pReq        Pointer to the request (returned by GMMR3AllocatePagesPrepare).
 */
GMMR3DECL(int) GMMR3AllocatePagesPerform(PVM pVM, PGMMALLOCATEPAGESREQ pReq)
{
    for (unsigned i = 0; ; i++)
    {
        int rc = SUPCallVMMR0Ex(pVM->pVMR0, VMMR0_DO_GMM_ALLOCATE_PAGES, 0, &pReq->Hdr);
        if (RT_SUCCESS(rc))
            return rc;
        if (rc != VERR_GMM_SEED_ME)
            return VMSetError(pVM, rc, RT_SRC_POS,
                              N_("GMMR0AllocatePages failed to allocate %u pages"),
                              pReq->cPages);
        Assert(i < pReq->cPages);

        /*
         * Seed another chunk.
         */
        void *pvChunk;
        rc = SUPPageAlloc(GMM_CHUNK_SIZE >> PAGE_SHIFT, &pvChunk);
        if (VBOX_FAILURE(rc))
            return VMSetError(pVM, rc, RT_SRC_POS,
                              N_("Out of memory (SUPPageAlloc) seeding a %u pages allocation request"),
                              pReq->cPages);

        rc = SUPCallVMMR0Ex(pVM->pVMR0, VMMR0_DO_GMM_SEED_CHUNK, (uintptr_t)pvChunk, NULL);
        if (RT_FAILURE(rc))
            return VMSetError(pVM, rc, RT_SRC_POS, N_("GMM seeding failed"));
    }
}


/**
 * Cleans up a GMMR0AllocatePages request.
 * @param   pReq        Pointer to the request (returned by GMMR3AllocatePagesPrepare).
 */
GMMR3DECL(void) GMMR3AllocatePagesCleanup(PGMMALLOCATEPAGESREQ pReq)
{
    RTMemTmpFree(pReq);
}


#if 0 /* impractical */
GMMR3DECL(int)  GMMR3FreePages(PVM pVM, uint32_t cPages, PGMMFREEPAGEDESC paPages, GMMACCOUNT enmAccount)
{
    GMMFREEPAGESREQ Req;
    Req.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    Req.Hdr.cbReq = sizeof(Req);

    return SUPCallVMMR0Ex(pVM->pVMR0, VMMR0_DO_GMM_FREE_PAGES, 0, &Req.Hdr);
}
#endif


#if 0 /* impractical */
GMMR3DECL(int)  GMMR3BalloonedPages(PVM pVM, uint32_t cBalloonedPages, uint32_t cPagesToFree, PGMMFREEPAGEDESC paPages, bool fCompleted)
{
    GMMBALLOONEDPAGESREQ Req;
    Req.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    Req.Hdr.cbReq = sizeof(Req);

    return SUPCallVMMR0Ex(pVM->pVMR0, VMMR0_DO_GMM_BALLOONED_PAGES, 0, &Req.Hdr);
}
#endif


/**
 * @see GMMR0DeflatedBalloon
 */
GMMR3DECL(int)  GMMR3DeflatedBalloon(PVM pVM, uint32_t cPages)
{
    return SUPCallVMMR0Ex(pVM->pVMR0, VMMR0_DO_GMM_DEFLATED_BALLOON, cPages, NULL);
}


/**
 * @see GMMR0MapUnmapChunk
 */
GMMR3DECL(int)  GMMR3MapUnmapChunk(PVM pVM, uint32_t idChunkMap, uint32_t idChunkUnmap, PRTR3PTR ppvR3)
{
    GMMMAPUNMAPCHUNKREQ Req;
    Req.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    Req.Hdr.cbReq = sizeof(Req);
    Req.idChunkMap = idChunkMap;
    Req.idChunkUnmap = idChunkUnmap;
    Req.pvR3 = NULL;
    int rc = SUPCallVMMR0Ex(pVM->pVMR0, VMMR0_DO_GMM_MAP_UNMAP_CHUNK, 0, &Req.Hdr);
    if (RT_SUCCESS(rc) && ppvR3)
        *ppvR3 = Req.pvR3;
    return rc;
}


/**
 * @see GMMR0SeedChunk
 */
GMMR3DECL(int)  GMMR3SeedChunk(PVM pVM, RTR3PTR pvR3)
{
    return SUPCallVMMR0Ex(pVM->pVMR0, VMMR0_DO_GMM_SEED_CHUNK, (uintptr_t)pvR3, NULL);
}

