/* $Id$ */
/** @file
 * IPRT - Ring-0 Memory Objects, Solaris.
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-solaris-kernel.h"
#include "internal/iprt.h"
#include <iprt/memobj.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/param.h>
#include <iprt/process.h>
#include "internal/memobj.h"
#include "memobj-r0drv-solaris.h"

#define SOL_IS_KRNL_ADDR(vx)    ((uintptr_t)(vx) >= kernelbase)
static vnode_t                  s_PageVnode;

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * The Solaris version of the memory object structure.
 */
typedef struct RTR0MEMOBJSOL
{
    /** The core structure. */
    RTR0MEMOBJINTERNAL  Core;
    /** Pointer to kernel memory cookie. */
    ddi_umem_cookie_t   Cookie;
    /** Shadow locked pages. */
    void               *pvHandle;
    /** Access during locking. */
    int                 fAccess;
    /** Set if large pages are involved in an RTR0MEMOBJTYPE_PHYS
     *  allocation. */
    bool                fLargePage;
} RTR0MEMOBJSOL, *PRTR0MEMOBJSOL;


/**
 * Returns the physical address for a virtual address.
 *
 * @param pv        The virtual address.
 *
 * @returns The physical address corresponding to @a pv.
 */
static uint64_t rtR0MemObjSolVirtToPhys(void *pv)
{
    struct hat *pHat         = NULL;
    pfn_t       PageFrameNum = 0;
    uintptr_t   uVirtAddr    = (uintptr_t)pv;

    if (SOL_IS_KRNL_ADDR(pv))
        pHat = kas.a_hat;
    else
    {
        proc_t *pProcess = (proc_t *)RTR0ProcHandleSelf();
        AssertRelease(pProcess);
        pHat = pProcess->p_as->a_hat;
    }

    PageFrameNum = hat_getpfnum(pHat, (caddr_t)(uVirtAddr & PAGEMASK));
    AssertReleaseMsg(PageFrameNum != PFN_INVALID, ("rtR0MemObjSolVirtToPhys failed. pv=%p\n", pv));
    return (((uint64_t)PageFrameNum << PAGESHIFT) | (uVirtAddr & PAGEOFFSET));
}


/**
 * Returns the physical address for a page.
 *
 * @param    pPage      Pointer to the page.
 *
 * @returns The physical address for a page.
 */
static inline uint64_t rtR0MemObjSolPagePhys(page_t *pPage)
{
    AssertPtr(pPage);
    pfn_t PageFrameNum = page_pptonum(pPage);
    AssertReleaseMsg(PageFrameNum != PFN_INVALID, ("rtR0MemObjSolPagePhys failed pPage=%p\n"));
    return (uint64_t)PageFrameNum << PAGESHIFT;
}


/**
 * Retreives a free page from the kernel freelist.
 *
 * @param virtAddr       The virtual address to which this page maybe mapped in
 *                       the future.
 * @param cbPage         The size of the page.
 *
 * @returns Pointer to the allocated page, NULL on failure.
 */
static page_t *rtR0MemObjSolPageFromFreelist(caddr_t virtAddr, size_t cbPage)
{
    seg_t KernelSeg;
    KernelSeg.s_as = &kas;
    page_t *pPage = page_get_freelist(&s_PageVnode, 0 /* offset */, &KernelSeg, virtAddr,
                                      cbPage, 0 /* flags */, NULL /* NUMA group */);
    if (   !pPage
        && g_frtSolUseKflt)
    {
        pPage = page_get_freelist(&s_PageVnode, 0 /* offset */, &KernelSeg, virtAddr,
                                  cbPage, PG_KFLT, NULL /* NUMA group */);
    }
    return pPage;
}


/**
 * Retrieves a free page from the kernel cachelist.
 *
 * @param virtAddr      The virtual address to which this page maybe mapped in
 *                      the future.
 * @param cbPage        The size of the page.
 *
 * @return Pointer to the allocated page, NULL on failure.
 */
static page_t *rtR0MemObjSolPageFromCachelist(caddr_t virtAddr, size_t cbPage)
{
    seg_t KernelSeg;
    KernelSeg.s_as = &kas;
    page_t *pPage = page_get_cachelist(&s_PageVnode, 0 /* offset */, &KernelSeg, virtAddr,
                                       0 /* flags */, NULL /* NUMA group */);
    if (   !pPage
        && g_frtSolUseKflt)
    {
        pPage = page_get_cachelist(&s_PageVnode, 0 /* offset */, &KernelSeg, virtAddr,
                                   PG_KFLT, NULL /* NUMA group */);
    }

    /*
     * Remove association with the vnode for pages from the cachelist.
     */
    if (!PP_ISAGED(pPage))
        page_hashout(pPage, NULL /* mutex */);

    return pPage;
}


/**
 * Allocates physical non-contiguous memory.
 *
 * @param uPhysHi   The upper physical address limit (inclusive).
 * @param puPhys    Where to store the physical address of first page. Optional,
 *                  can be NULL.
 * @param cb        The size of the allocation.
 *
 * @return Array of allocated pages, NULL on failure.
 */
static page_t **rtR0MemObjSolPagesAlloc(uint64_t uPhysHi, uint64_t *puPhys, size_t cb)
{
    /*
     * The page freelist and cachelist both hold pages that are not mapped into any address space.
     * The cachelist is not really free pages but when memory is exhausted they'll be moved to the
     * free lists, it's the total of the free+cache list that we see on the 'free' column in vmstat.
     *
     * Reserve available memory for pages and create the pages.
     */
    pgcnt_t cPages = (cb + PAGESIZE - 1) >> PAGESHIFT;
    int rc = page_resv(cPages, KM_NOSLEEP);
    if (rc)
    {
        rc = page_create_wait(cPages, 0 /* flags */);
        if (rc)
        {
            size_t   cbPages = cPages * sizeof(page_t *);
            page_t **ppPages = kmem_zalloc(cbPages, KM_SLEEP);
            if (RT_LIKELY(ppPages))
            {
                /*
                 * Get pages from kseg, the 'virtAddr' here is only for colouring but unfortunately
                 * we don't yet have the 'virtAddr' to which this memory may be mapped.
                 */
                caddr_t virtAddr = NULL;
                for (size_t i = 0; i < cPages; i++, virtAddr += PAGESIZE)
                {
                    uint32_t cTries = 3;
                    page_t *pPage   = NULL;
                    while (cTries > 0)
                    {
                        /*
                         * Get a page from the freelist or cachelist & verify if it's within our
                         * requested range.
                         */
                        pPage = rtR0MemObjSolPageFromFreelist(virtAddr, PAGESIZE);
                        if (!pPage)
                        {
                            pPage = rtR0MemObjSolPageFromCachelist(virtAddr, PAGESIZE);
                            if (RT_UNLIKELY(!pPage))
                                break;
                        }
                        if (uPhysHi != NIL_RTHCPHYS)
                        {
                            uint64_t uPhys = rtR0MemObjSolPagePhys(pPage);
                            if (uPhys > uPhysHi)
                            {
                                page_free(pPage, 0 /* don't need page, move to tail of pagelist */);
                                pPage = NULL;
                                --cTries;
                                continue;
                            }
                        }

                        PP_CLRFREE(pPage);      /* Page is no longer free */
                        PP_CLRAGED(pPage);      /* Page is not hashed in */
                        ppPages[i] = pPage;
                        break;
                    }

                    if (RT_UNLIKELY(!pPage))
                    {
                        /*
                         * No pages found or found pages didn't meet requirements, release what was grabbed so far.
                         */
                        page_create_putback(cPages - i);
                        while (--i >= 0)
                            page_free(ppPages[i], 0 /* don't need page, move to tail of pagelist */);
                        kmem_free(ppPages, cbPages);
                        page_unresv(cPages);
                        return NULL;
                    }
                }

                /*
                 * We now have the pages locked exclusively, before they are mapped in
                 * we must downgrade the lock.
                 */
                if (puPhys)
                    *puPhys = rtR0MemObjSolPagePhys(ppPages[0]);
                return ppPages;
            }

            page_create_putback(cPages);
        }

        page_unresv(cPages);
    }

    return NULL;
}


/**
 * Prepares pages allocated by rtR0MemObjSolPagesAlloc for mapping.
 *
 * @param    ppPages    Pointer to the page list.
 * @param    cb         Size of the allocation.
 * @param    auPhys     Where to store the physical address of the premapped
 *                      pages.
 * @param    cPages     The number of pages (entries) in @a auPhys.
 *
 * @returns IPRT status code.
 */
static int rtR0MemObjSolPagesPreMap(page_t **ppPages, size_t cb, uint64_t auPhys[], size_t cPages)
{
    AssertPtrReturn(ppPages, VERR_INVALID_PARAMETER);
    AssertPtrReturn(auPhys, VERR_INVALID_PARAMETER);

    for (size_t iPage = 0; iPage < cPages; iPage++)
    {
        /*
         * Prepare pages for mapping into kernel/user-space. Downgrade the
         * exclusive page lock to a shared lock to prevent page relocation.
         */
        if (page_tryupgrade(ppPages[iPage]) == 1)
            page_downgrade(ppPages[iPage]);

        auPhys[iPage] = rtR0MemObjSolPagePhys(ppPages[iPage]);
    }

    return VINF_SUCCESS;
}


/**
 * Frees pages allocated by rtR0MemObjSolPagesAlloc.
 *
 * @param ppPages       Pointer to the page list.
 * @param cbPages       Size of the allocation.
 */
static void rtR0MemObjSolPagesFree(page_t **ppPages, size_t cb)
{
    size_t cPages  = (cb + PAGESIZE - 1) >> PAGESHIFT;
    size_t cbPages = cPages * sizeof(page_t *);
    for (size_t iPage = 0; iPage < cPages; iPage++)
    {
        /*
         *  We need to exclusive lock the pages before freeing them.
         */
        int rc = page_tryupgrade(ppPages[iPage]);
        if (!rc)
        {
            page_unlock(ppPages[iPage]);
            while (!page_lock(ppPages[iPage], SE_EXCL, NULL /* mutex */, P_RECLAIM))
            {
                /* nothing */;
            }
        }
        page_free(ppPages[iPage], 0 /* don't need page, move to tail of pagelist */);
    }
    kmem_free(ppPages, cbPages);
    page_unresv(cPages);
}


/**
 * Allocates a large page to cover the required allocation size.
 *
 * @param puPhys        Where to store the physical address of the allocated
 *                      page. Optional, can be NULL.
 * @param cb            Size of the allocation.
 *
 * @returns Pointer to the allocated large page, NULL on failure.
 */
static page_t *rtR0MemObjSolLargePageAlloc(uint64_t *puPhys, size_t cb)
{
    /*
     * Reserve available memory and create the sub-pages.
     */
    const pgcnt_t cPages = cb >> PAGESHIFT;
    int rc = page_resv(cPages, KM_NOSLEEP);
    if (rc)
    {
        rc = page_create_wait(cPages, 0 /* flags */);
        if (rc)
        {
            /*
             * Get a page off the free list. We set virtAddr to 0 since we don't know where
             * the memory is going to be mapped.
             */
            seg_t KernelSeg;
            caddr_t virtAddr  = NULL;
            KernelSeg.s_as    = &kas;
            page_t *pRootPage = rtR0MemObjSolPageFromFreelist(virtAddr, cb);
            if (pRootPage)
            {
                AssertMsg(!(page_pptonum(pRootPage) & (cPages - 1)), ("%p:%lx cPages=%lx\n", pRootPage, page_pptonum(pRootPage), cPages));

                /*
                 * Mark all the sub-pages as non-free and not-hashed-in.
                 * It is paramount that we destroy the list (before freeing it).
                 */
                page_t *pPageList = pRootPage;
                for (size_t iPage = 0; iPage < cPages; iPage++)
                {
                    page_t *pPage = pPageList;
                    AssertPtr(pPage);
                    AssertMsg(page_pptonum(pPage) == iPage + page_pptonum(pRootPage),
                        ("%p:%lx %lx+%lx\n", pPage, page_pptonum(pPage), iPage, page_pptonum(pRootPage)));
                    page_sub(&pPageList, pPage);

                    /*
                     * Ensure page is now be free and the page size-code must match that of the root page.
                     */
                    AssertMsg(PP_ISFREE(pPage), ("%p\n", pPage));
                    AssertMsg(pPage->p_szc == pRootPage->p_szc, ("%p - %d expected %d \n", pPage, pPage->p_szc, pRootPage->p_szc));

                    PP_CLRFREE(pPage);      /* Page no longer free */
                    PP_CLRAGED(pPage);      /* Page no longer hashed-in */
                }

                uint64_t uPhys = rtR0MemObjSolPagePhys(pRootPage);
                AssertMsg(!(uPhys & (cb - 1)), ("%llx %zx\n", uPhys, cb));
                if (puPhys)
                    *puPhys = uPhys;

                return pRootPage;
            }

            page_create_putback(cPages);
        }

        page_unresv(cPages);
    }

    return NULL;
}


/**
 * Prepares the large page allocated by rtR0MemObjSolLargePageAlloc to be mapped.
 *
 * @param    pRootPage      Pointer to the root page.
 * @param    cb             Size of the allocation.
 *
 * @returns IPRT status code.
 */
static int rtR0MemObjSolLargePagePreMap(page_t *pRootPage, size_t cb)
{
    const pgcnt_t cPages = cb >> PAGESHIFT;

    Assert(page_get_pagecnt(pRootPage->p_szc) == cPages);
    AssertMsg(!(page_pptonum(pRootPage) & (cPages - 1)), ("%p:%lx npages=%lx\n", pRootPage, page_pptonum(pRootPage), cPages));

    /*
     * We need to downgrade the sub-pages from exclusive to shared locking
     * to prevent page relocation.
     */
    for (pgcnt_t iPage = 0; iPage < cPages; iPage++)
    {
        page_t *pPage = page_nextn(pRootPage, iPage);
        AssertMsg(page_pptonum(pPage) == iPage + page_pptonum(pRootPage),
            ("%p:%lx %lx+%lx\n", pPage, page_pptonum(pPage), iPage, page_pptonum(pRootPage)));
        AssertMsg(!PP_ISFREE(pPage), ("%p\n", pPage));

        if (page_tryupgrade(pPage) == 1)
            page_downgrade(pPage);
        AssertMsg(!PP_ISFREE(pPage), ("%p\n", pPage));
    }

    return VINF_SUCCESS;
}


/**
 * Frees the page allocated by rtR0MemObjSolLargePageAlloc.
 *
 * @param    pRootPage      Pointer to the root page.
 * @param    cb             Allocated size.
 */
static void rtR0MemObjSolLargePageFree(page_t *pRootPage, size_t cb)
{
    pgcnt_t cPages = cb >> PAGESHIFT;

    Assert(page_get_pagecnt(pRootPage->p_szc) == cPages);
    AssertMsg(!(page_pptonum(pRootPage) & (cPages - 1)), ("%p:%lx cPages=%lx\n", pRootPage, page_pptonum(pRootPage), cPages));

    /*
     * We need to exclusively lock the sub-pages before freeing the large one.
     */
    for (pgcnt_t iPage = 0; iPage < cPages; iPage++)
    {
        page_t *pPage = page_nextn(pRootPage, iPage);
        AssertMsg(page_pptonum(pPage) == iPage + page_pptonum(pRootPage),
                  ("%p:%lx %lx+%lx\n", pPage, page_pptonum(pPage), iPage, page_pptonum(pRootPage)));
        AssertMsg(!PP_ISFREE(pPage), ("%p\n", pPage));

        int rc = page_tryupgrade(pPage);
        if (!rc)
        {
            page_unlock(pPage);
            while (!page_lock(pPage, SE_EXCL, NULL /* mutex */, P_RECLAIM))
            {
                /* nothing */;
            }
        }
    }

    /*
     * Free the large page and unreserve the memory.
     */
    page_free_pages(pRootPage);
    page_unresv(cPages);

}


/**
 * Unmaps kernel/user-space mapped memory.
 *
 * @param    pv         Pointer to the mapped memory block.
 * @param    cb         Size of the memory block.
 */
static void rtR0MemObjSolUnmap(void *pv, size_t cb)
{
    if (SOL_IS_KRNL_ADDR(pv))
    {
        hat_unload(kas.a_hat, pv, cb, HAT_UNLOAD | HAT_UNLOAD_UNLOCK);
        vmem_free(heap_arena, pv, cb);
    }
    else
    {
        struct as *pAddrSpace = ((proc_t *)RTR0ProcHandleSelf())->p_as;
        AssertPtr(pAddrSpace);
        as_rangelock(pAddrSpace);
        as_unmap(pAddrSpace, pv, cb);
        as_rangeunlock(pAddrSpace);
    }
}


/**
 * Lock down memory mappings for a virtual address.
 *
 * @param    pv             Pointer to the memory to lock down.
 * @param    cb             Size of the memory block.
 * @param    fAccess        Page access rights (S_READ, S_WRITE, S_EXEC)
 *
 * @returns IPRT status code.
 */
static int rtR0MemObjSolLock(void *pv, size_t cb, int fPageAccess)
{
    /*
     * Kernel memory mappings on x86/amd64 are always locked, only handle user-space memory.
     */
    if (!SOL_IS_KRNL_ADDR(pv))
    {
        proc_t *pProc = (proc_t *)RTR0ProcHandleSelf();
        AssertPtr(pProc);
        faultcode_t rc = as_fault(pProc->p_as->a_hat, pProc->p_as, (caddr_t)pv, cb, F_SOFTLOCK, fPageAccess);
        if (rc)
        {
            LogRel(("rtR0MemObjSolLock failed for pv=%pv cb=%lx fPageAccess=%d rc=%d\n", pv, cb, fPageAccess, rc));
            return VERR_LOCK_FAILED;
        }
    }
    return VINF_SUCCESS;
}


/**
 * Unlock memory mappings for a virtual address.
 *
 * @param    pv             Pointer to the locked memory.
 * @param    cb             Size of the memory block.
 * @param    fPageAccess    Page access rights (S_READ, S_WRITE, S_EXEC).
 */
static void rtR0MemObjSolUnlock(void *pv, size_t cb, int fPageAccess)
{
    if (!SOL_IS_KRNL_ADDR(pv))
    {
        proc_t *pProcess = (proc_t *)RTR0ProcHandleSelf();
        AssertPtr(pProcess);
        as_fault(pProcess->p_as->a_hat, pProcess->p_as, (caddr_t)pv, cb, F_SOFTUNLOCK, fPageAccess);
    }
}


/**
 * Maps a list of physical pages into user address space.
 *
 * @param    pVirtAddr      Where to store the virtual address of the mapping.
 * @param    fPageAccess    Page access rights (PROT_READ, PROT_WRITE,
 *                          PROT_EXEC)
 * @param    paPhysAddrs    Array of physical addresses to pages.
 * @param    cb             Size of memory being mapped.
 *
 * @returns IPRT status code.
 */
static int rtR0MemObjSolUserMap(caddr_t *pVirtAddr, unsigned fPageAccess, uint64_t *paPhysAddrs, size_t cb)
{
    struct as *pAddrSpace = ((proc_t *)RTR0ProcHandleSelf())->p_as;
    int rc = VERR_INTERNAL_ERROR;
    SEGVBOX_CRARGS Args;

    Args.paPhysAddrs = paPhysAddrs;
    Args.fPageAccess = fPageAccess;

    as_rangelock(pAddrSpace);
    map_addr(pVirtAddr, cb, 0 /* offset */, 0 /* vacalign */, MAP_SHARED);
    if (*pVirtAddr != NULL)
        rc = as_map(pAddrSpace, *pVirtAddr, cb, rtR0SegVBoxSolCreate, &Args);
    else
        rc = ENOMEM;
    as_rangeunlock(pAddrSpace);

    return RTErrConvertFromErrno(rc);
}


DECLHIDDEN(int) rtR0MemObjNativeFree(RTR0MEMOBJ pMem)
{
    PRTR0MEMOBJSOL pMemSolaris = (PRTR0MEMOBJSOL)pMem;

    switch (pMemSolaris->Core.enmType)
    {
        case RTR0MEMOBJTYPE_LOW:
            rtR0SolMemFree(pMemSolaris->Core.pv, pMemSolaris->Core.cb);
            break;

        case RTR0MEMOBJTYPE_PHYS:
            if (pMemSolaris->Core.u.Phys.fAllocated)
            {
                if (pMemSolaris->fLargePage)
                    rtR0MemObjSolLargePageFree(pMemSolaris->pvHandle, pMemSolaris->Core.cb);
                else
                    rtR0SolMemFree(pMemSolaris->Core.pv, pMemSolaris->Core.cb);
            }
            break;

        case RTR0MEMOBJTYPE_PHYS_NC:
            rtR0MemObjSolPagesFree(pMemSolaris->pvHandle, pMemSolaris->Core.cb);
            break;

        case RTR0MEMOBJTYPE_PAGE:
            ddi_umem_free(pMemSolaris->Cookie);
            break;

        case RTR0MEMOBJTYPE_LOCK:
            rtR0MemObjSolUnlock(pMemSolaris->Core.pv, pMemSolaris->Core.cb, pMemSolaris->fAccess);
            break;

        case RTR0MEMOBJTYPE_MAPPING:
            rtR0MemObjSolUnmap(pMemSolaris->Core.pv, pMemSolaris->Core.cb);
            break;

        case RTR0MEMOBJTYPE_RES_VIRT:
        {
            if (pMemSolaris->Core.u.ResVirt.R0Process == NIL_RTR0PROCESS)
                vmem_xfree(heap_arena, pMemSolaris->Core.pv, pMemSolaris->Core.cb);
            else
                AssertFailed();
            break;
        }

        case RTR0MEMOBJTYPE_CONT: /* we don't use this type here. */
        default:
            AssertMsgFailed(("enmType=%d\n", pMemSolaris->Core.enmType));
            return VERR_INTERNAL_ERROR;
    }

    return VINF_SUCCESS;
}


DECLHIDDEN(int) rtR0MemObjNativeAllocPage(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    /* Create the object. */
    PRTR0MEMOBJSOL pMemSolaris = (PRTR0MEMOBJSOL)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_PAGE, NULL, cb);
    if (RT_UNLIKELY(!pMemSolaris))
        return VERR_NO_MEMORY;

    void *pvMem = ddi_umem_alloc(cb, DDI_UMEM_SLEEP, &pMemSolaris->Cookie);
    if (RT_UNLIKELY(!pvMem))
    {
        rtR0MemObjDelete(&pMemSolaris->Core);
        return VERR_NO_PAGE_MEMORY;
    }

    pMemSolaris->Core.pv  = pvMem;
    pMemSolaris->pvHandle = NULL;
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
}


DECLHIDDEN(int) rtR0MemObjNativeAllocLow(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    NOREF(fExecutable);

    /* Create the object */
    PRTR0MEMOBJSOL pMemSolaris = (PRTR0MEMOBJSOL)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_LOW, NULL, cb);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    /* Allocate physically low page-aligned memory. */
    uint64_t uPhysHi = _4G - 1;
    void *pvMem = rtR0SolMemAlloc(uPhysHi, NULL /* puPhys */, cb, PAGESIZE, false /* fContig */);
    if (RT_UNLIKELY(!pvMem))
    {
        rtR0MemObjDelete(&pMemSolaris->Core);
        return VERR_NO_LOW_MEMORY;
    }
    pMemSolaris->Core.pv = pvMem;
    pMemSolaris->pvHandle = NULL;
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
}


DECLHIDDEN(int) rtR0MemObjNativeAllocCont(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    NOREF(fExecutable);
    return rtR0MemObjNativeAllocPhys(ppMem, cb, _4G - 1, PAGE_SIZE /* alignment */);
}


DECLHIDDEN(int) rtR0MemObjNativeAllocPhysNC(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, RTHCPHYS PhysHighest)
{
#if HC_ARCH_BITS == 64
    PRTR0MEMOBJSOL pMemSolaris = (PRTR0MEMOBJSOL)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_PHYS_NC, NULL, cb);
    if (RT_UNLIKELY(!pMemSolaris))
        return VERR_NO_MEMORY;

    uint64_t PhysAddr = UINT64_MAX;
    void *pvPages = rtR0MemObjSolPagesAlloc((uint64_t)PhysHighest, &PhysAddr, cb);
    if (!pvPages)
    {
        LogRel(("rtR0MemObjNativeAllocPhysNC: rtR0MemObjSolPagesAlloc failed for cb=%u.\n", cb));
        rtR0MemObjDelete(&pMemSolaris->Core);
        return VERR_NO_MEMORY;
    }
    pMemSolaris->Core.pv   = NULL;
    pMemSolaris->pvHandle  = pvPages;

    Assert(PhysAddr != UINT64_MAX);
    Assert(!(PhysAddr & PAGE_OFFSET_MASK));
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;

#else /* 32 bit: */
    return VERR_NOT_SUPPORTED; /* see the RTR0MemObjAllocPhysNC specs */
#endif
}


DECLHIDDEN(int) rtR0MemObjNativeAllocPhys(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, RTHCPHYS PhysHighest, size_t uAlignment)
{
    AssertMsgReturn(PhysHighest >= 16 *_1M, ("PhysHigest=%RHp\n", PhysHighest), VERR_NOT_SUPPORTED);

    PRTR0MEMOBJSOL pMemSolaris = (PRTR0MEMOBJSOL)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_PHYS, NULL, cb);
    if (RT_UNLIKELY(!pMemSolaris))
        return VERR_NO_MEMORY;

    /*
     * Allocating one large page gets special treatment.
     */
    static uint32_t s_cbLargePage = UINT32_MAX;
    if (s_cbLargePage == UINT32_MAX)
    {
#if 0 /* currently not entirely stable, so disabled. */
        if (page_num_pagesizes() > 1)
            ASMAtomicWriteU32(&s_cbLargePage, page_get_pagesize(1));
        else
#endif
            ASMAtomicWriteU32(&s_cbLargePage, 0);
    }
    uint64_t PhysAddr;
    if (   cb == s_cbLargePage
        && cb == uAlignment
        && PhysHighest == NIL_RTHCPHYS)
    {
        /*
         * Allocate one large page.
         */
        void *pvPages = rtR0MemObjSolLargePageAlloc(&PhysAddr, cb);
        if (RT_LIKELY(pvPages))
        {
            AssertMsg(!(PhysAddr & (cb - 1)), ("%RHp\n", PhysAddr));
            pMemSolaris->Core.pv                = NULL;
            pMemSolaris->Core.u.Phys.PhysBase   = PhysAddr;
            pMemSolaris->Core.u.Phys.fAllocated = true;
            pMemSolaris->pvHandle               = pvPages;
            pMemSolaris->fLargePage             = true;

            *ppMem = &pMemSolaris->Core;
            return VINF_SUCCESS;
        }
    }
    else
    {
        /*
         * Allocate physically contiguous memory aligned as specified.
         */
        AssertCompile(NIL_RTHCPHYS == UINT64_MAX);
        PhysAddr = PhysHighest;
        void *pvMem = rtR0SolMemAlloc(PhysHighest, &PhysAddr, cb, uAlignment, true /* fContig */);
        if (RT_LIKELY(pvMem))
        {
            Assert(!(PhysAddr & PAGE_OFFSET_MASK));
            Assert(PhysAddr < PhysHighest);
            Assert(PhysAddr + cb <= PhysHighest);

            pMemSolaris->Core.pv                = pvMem;
            pMemSolaris->Core.u.Phys.PhysBase   = PhysAddr;
            pMemSolaris->Core.u.Phys.fAllocated = true;
            pMemSolaris->pvHandle               = NULL;
            pMemSolaris->fLargePage             = false;

            *ppMem = &pMemSolaris->Core;
            return VINF_SUCCESS;
        }
    }
    rtR0MemObjDelete(&pMemSolaris->Core);
    return VERR_NO_CONT_MEMORY;
}


DECLHIDDEN(int) rtR0MemObjNativeEnterPhys(PPRTR0MEMOBJINTERNAL ppMem, RTHCPHYS Phys, size_t cb, uint32_t uCachePolicy)
{
    AssertReturn(uCachePolicy == RTMEM_CACHE_POLICY_DONT_CARE, VERR_NOT_SUPPORTED);

    /* Create the object. */
    PRTR0MEMOBJSOL pMemSolaris = (PRTR0MEMOBJSOL)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_PHYS, NULL, cb);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    /* There is no allocation here, it needs to be mapped somewhere first. */
    pMemSolaris->Core.u.Phys.fAllocated   = false;
    pMemSolaris->Core.u.Phys.PhysBase     = Phys;
    pMemSolaris->Core.u.Phys.uCachePolicy = uCachePolicy;
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
}


DECLHIDDEN(int) rtR0MemObjNativeLockUser(PPRTR0MEMOBJINTERNAL ppMem, RTR3PTR R3Ptr, size_t cb, uint32_t fAccess,
                                         RTR0PROCESS R0Process)
{
    AssertReturn(R0Process == RTR0ProcHandleSelf(), VERR_INVALID_PARAMETER);
    NOREF(fAccess);

    /* Create the locking object */
    PRTR0MEMOBJSOL pMemSolaris = (PRTR0MEMOBJSOL)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_LOCK, (void *)R3Ptr, cb);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    /* Lock down user pages. */
    int fPageAccess = S_READ;
    if (fAccess & RTMEM_PROT_WRITE)
        fPageAccess = S_WRITE;
    if (fAccess & RTMEM_PROT_EXEC)
        fPageAccess = S_EXEC;
    int rc = rtR0MemObjSolLock((void *)R3Ptr, cb, fPageAccess);
    if (RT_FAILURE(rc))
    {
        LogRel(("rtR0MemObjNativeLockUser: rtR0MemObjSolLock failed rc=%d\n", rc));
        rtR0MemObjDelete(&pMemSolaris->Core);
        return rc;
    }

    /* Fill in the object attributes and return successfully. */
    pMemSolaris->Core.u.Lock.R0Process  = R0Process;
    pMemSolaris->pvHandle               = NULL;
    pMemSolaris->fAccess                = fPageAccess;
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
}


DECLHIDDEN(int) rtR0MemObjNativeLockKernel(PPRTR0MEMOBJINTERNAL ppMem, void *pv, size_t cb, uint32_t fAccess)
{
    NOREF(fAccess);

    PRTR0MEMOBJSOL pMemSolaris = (PRTR0MEMOBJSOL)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_LOCK, pv, cb);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    /* Lock down kernel pages. */
    int fPageAccess = S_READ;
    if (fAccess & RTMEM_PROT_WRITE)
        fPageAccess = S_WRITE;
    if (fAccess & RTMEM_PROT_EXEC)
        fPageAccess = S_EXEC;
    int rc = rtR0MemObjSolLock(pv, cb, fPageAccess);
    if (RT_FAILURE(rc))
    {
        LogRel(("rtR0MemObjNativeLockKernel: rtR0MemObjSolLock failed rc=%d\n", rc));
        rtR0MemObjDelete(&pMemSolaris->Core);
        return rc;
    }

    /* Fill in the object attributes and return successfully. */
    pMemSolaris->Core.u.Lock.R0Process = NIL_RTR0PROCESS;
    pMemSolaris->pvHandle              = NULL;
    pMemSolaris->fAccess               = fPageAccess;
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
}


DECLHIDDEN(int) rtR0MemObjNativeReserveKernel(PPRTR0MEMOBJINTERNAL ppMem, void *pvFixed, size_t cb, size_t uAlignment)
{
    PRTR0MEMOBJSOL  pMemSolaris;

    /*
     * Use xalloc.
     */
    void *pv = vmem_xalloc(heap_arena, cb, uAlignment, 0 /* phase */, 0 /* nocross */,
                           NULL /* minaddr */, NULL /* maxaddr */, VM_SLEEP);
    if (RT_UNLIKELY(!pv))
        return VERR_NO_MEMORY;

    /* Create the object. */
    pMemSolaris = (PRTR0MEMOBJSOL)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_RES_VIRT, pv, cb);
    if (!pMemSolaris)
    {
        LogRel(("rtR0MemObjNativeReserveKernel failed to alloc memory object.\n"));
        vmem_xfree(heap_arena, pv, cb);
        return VERR_NO_MEMORY;
    }

    pMemSolaris->Core.u.ResVirt.R0Process = NIL_RTR0PROCESS;
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
}


DECLHIDDEN(int) rtR0MemObjNativeReserveUser(PPRTR0MEMOBJINTERNAL ppMem, RTR3PTR R3PtrFixed, size_t cb, size_t uAlignment, RTR0PROCESS R0Process)
{
    return VERR_NOT_SUPPORTED;
}


DECLHIDDEN(int) rtR0MemObjNativeMapKernel(PPRTR0MEMOBJINTERNAL ppMem, RTR0MEMOBJ pMemToMap, void *pvFixed, size_t uAlignment,
                                          unsigned fProt, size_t offSub, size_t cbSub)
{
    /** @todo rtR0MemObjNativeMapKernel / Solaris - Should be fairly simple alloc kernel memory and memload it. */
    return VERR_NOT_SUPPORTED;
}


DECLHIDDEN(int) rtR0MemObjNativeMapUser(PPRTR0MEMOBJINTERNAL ppMem, PRTR0MEMOBJINTERNAL pMemToMap, RTR3PTR R3PtrFixed,
                                        size_t uAlignment, unsigned fProt, RTR0PROCESS R0Process)
{
    /*
     * Fend off things we cannot do.
     */
    AssertMsgReturn(R3PtrFixed == (RTR3PTR)-1, ("%p\n", R3PtrFixed), VERR_NOT_SUPPORTED);
    AssertMsgReturn(R0Process == RTR0ProcHandleSelf(), ("%p != %p\n", R0Process, RTR0ProcHandleSelf()), VERR_NOT_SUPPORTED);
    if (uAlignment != PAGE_SIZE)
        return VERR_NOT_SUPPORTED;

    /*
     * Get parameters from the source object.
     */
    PRTR0MEMOBJSOL  pMemToMapSolaris     = (PRTR0MEMOBJSOL)pMemToMap;
    void           *pv                   = pMemToMapSolaris->Core.pv;
    size_t          cb                   = pMemToMapSolaris->Core.cb;
    size_t          cPages               = cb >> PAGE_SHIFT;

    /*
     * Create the mapping object
     */
    PRTR0MEMOBJSOL pMemSolaris;
    pMemSolaris = (PRTR0MEMOBJSOL)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_MAPPING, pv, cb);
    if (RT_UNLIKELY(!pMemSolaris))
        return VERR_NO_MEMORY;

    int rc = VINF_SUCCESS;
    uint64_t *paPhysAddrs = kmem_zalloc(sizeof(uint64_t) * cPages, KM_SLEEP);
    if (RT_LIKELY(paPhysAddrs))
    {
        /*
         * Prepare the pages according to type.
         */
        if (pMemToMapSolaris->Core.enmType == RTR0MEMOBJTYPE_PHYS_NC)
            rc = rtR0MemObjSolPagesPreMap(pMemToMapSolaris->pvHandle, cb, paPhysAddrs, cPages);
        else if (   pMemToMapSolaris->Core.enmType == RTR0MEMOBJTYPE_PHYS
                 && pMemToMapSolaris->fLargePage)
        {
            RTHCPHYS Phys = pMemToMapSolaris->Core.u.Phys.PhysBase;
            for (pgcnt_t iPage = 0; iPage < cPages; iPage++, Phys += PAGE_SIZE)
                paPhysAddrs[iPage] = Phys;
            rc = rtR0MemObjSolLargePagePreMap(pMemToMapSolaris->pvHandle, cb);
        }
        else
        {
            /*
             * Have kernel mapping, just translate virtual to physical.
             */
            AssertPtr(pv);
            rc = VINF_SUCCESS;
            for (size_t iPage = 0; iPage < cPages; iPage++)
            {
                paPhysAddrs[iPage] = rtR0MemObjSolVirtToPhys(pv);
                if (RT_UNLIKELY(paPhysAddrs[iPage] == -(uint64_t)1))
                {
                    LogRel(("rtR0MemObjNativeMapUser: no page to map.\n"));
                    rc = VERR_MAP_FAILED;
                    break;
                }
                pv = (void *)((uintptr_t)pv + PAGE_SIZE);
            }
        }
        if (RT_SUCCESS(rc))
        {
            unsigned fPageAccess = PROT_READ;
            if (fProt & RTMEM_PROT_WRITE)
                fPageAccess |= PROT_WRITE;
            if (fProt & RTMEM_PROT_EXEC)
                fPageAccess |= PROT_EXEC;

            /*
             * Perform the actual mapping.
             */
            caddr_t UserAddr = NULL;
            rc = rtR0MemObjSolUserMap(&UserAddr, fPageAccess, paPhysAddrs, cb);
            if (RT_SUCCESS(rc))
            {
                pMemSolaris->Core.u.Mapping.R0Process = R0Process;
                pMemSolaris->Core.pv                  = UserAddr;

                *ppMem = &pMemSolaris->Core;
                kmem_free(paPhysAddrs, sizeof(uint64_t) * cPages);
                return VINF_SUCCESS;
            }

            LogRel(("rtR0MemObjNativeMapUser: rtR0MemObjSolUserMap failed rc=%d.\n", rc));
        }

        rc = VERR_MAP_FAILED;
        kmem_free(paPhysAddrs, sizeof(uint64_t) * cPages);
    }
    else
        rc = VERR_NO_MEMORY;
    rtR0MemObjDelete(&pMemSolaris->Core);
    return rc;
}


DECLHIDDEN(int) rtR0MemObjNativeProtect(PRTR0MEMOBJINTERNAL pMem, size_t offSub, size_t cbSub, uint32_t fProt)
{
    NOREF(pMem);
    NOREF(offSub);
    NOREF(cbSub);
    NOREF(fProt);
    return VERR_NOT_SUPPORTED;
}


DECLHIDDEN(RTHCPHYS) rtR0MemObjNativeGetPagePhysAddr(PRTR0MEMOBJINTERNAL pMem, size_t iPage)
{
    PRTR0MEMOBJSOL pMemSolaris = (PRTR0MEMOBJSOL)pMem;

    switch (pMemSolaris->Core.enmType)
    {
        case RTR0MEMOBJTYPE_PHYS_NC:
            if (pMemSolaris->Core.u.Phys.fAllocated)
            {
                uint8_t *pb = (uint8_t *)pMemSolaris->Core.pv + ((size_t)iPage << PAGE_SHIFT);
                return rtR0MemObjSolVirtToPhys(pb);
            }
            page_t **ppPages = pMemSolaris->pvHandle;
            return rtR0MemObjSolPagePhys(ppPages[iPage]);

        case RTR0MEMOBJTYPE_PAGE:
        case RTR0MEMOBJTYPE_LOW:
        case RTR0MEMOBJTYPE_LOCK:
        {
            uint8_t *pb = (uint8_t *)pMemSolaris->Core.pv + ((size_t)iPage << PAGE_SHIFT);
            return rtR0MemObjSolVirtToPhys(pb);
        }

        /*
         * Although mapping can be handled by rtR0MemObjSolVirtToPhys(offset) like the above case,
         * request it from the parent so that we have a clear distinction between CONT/PHYS_NC.
         */
        case RTR0MEMOBJTYPE_MAPPING:
            return rtR0MemObjNativeGetPagePhysAddr(pMemSolaris->Core.uRel.Child.pParent, iPage);

        case RTR0MEMOBJTYPE_CONT:
        case RTR0MEMOBJTYPE_PHYS:
            AssertFailed(); /* handled by the caller */
        case RTR0MEMOBJTYPE_RES_VIRT:
        default:
            return NIL_RTHCPHYS;
    }
}

