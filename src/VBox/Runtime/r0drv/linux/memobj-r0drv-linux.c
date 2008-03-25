/* $Revision$ */
/** @file
 * innotek Portable Runtime - Ring-0 Memory Objects, Linux.
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
#include "the-linux-kernel.h"

#include <iprt/memobj.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/log.h>
#include <iprt/string.h>
#include <iprt/process.h>
#include "internal/memobj.h"

/* early 2.6 kernels */
#ifndef PAGE_SHARED_EXEC
# define PAGE_SHARED_EXEC PAGE_SHARED
#endif
#ifndef PAGE_READONLY_EXEC
# define PAGE_READONLY_EXEC PAGE_READONLY
#endif


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * The Darwin version of the memory object structure.
 */
typedef struct RTR0MEMOBJLNX
{
    /** The core structure. */
    RTR0MEMOBJINTERNAL  Core;
    /** Set if the allocation is contiguous.
     * This means it has to be given back as one chunk. */
    bool                fContiguous;
    /** Set if we've vmap'ed thed memory into ring-0. */
    bool                fMappedToRing0;
    /** The pages in the apPages array. */
    size_t              cPages;
    /** Array of struct page pointers. (variable size) */
    struct page        *apPages[1];
} RTR0MEMOBJLNX, *PRTR0MEMOBJLNX;


/**
 * Helper that converts from a RTR0PROCESS handle to a linux task.
 *
 * @returns The corresponding Linux task.
 * @param   R0Process   IPRT ring-0 process handle.
 */
struct task_struct *rtR0ProcessToLinuxTask(RTR0PROCESS R0Process)
{
    /** @todo fix rtR0ProcessToLinuxTask!! */
    return R0Process == RTR0ProcHandleSelf() ? current : NULL;
}


/**
 * Compute order. Some functions allocate 2^order pages.
 *
 * @returns order.
 * @param   cPages      Number of pages.
 */
static int rtR0MemObjLinuxOrder(size_t cPages)
{
    int     iOrder;
    size_t  cTmp;

    for (iOrder = 0, cTmp = cPages; cTmp >>= 1; ++iOrder)
        ;
    if (cPages & ~((size_t)1 << iOrder))
        ++iOrder;

    return iOrder;
}


/**
 * Converts from RTMEM_PROT_* to Linux PAGE_*.
 *
 * @returns Linux page protection constant.
 * @param   fProt       The IPRT protection mask.
 * @param   fKernel     Whether it applies to kernel or user space.
 */
static pgprot_t rtR0MemObjLinuxConvertProt(unsigned fProt, bool fKernel)
{
    switch (fProt)
    {
        default:
            AssertMsgFailed(("%#x %d\n", fProt, fKernel));
        case RTMEM_PROT_NONE:
            return PAGE_NONE;

        case RTMEM_PROT_READ:
            return fKernel ? PAGE_KERNEL_RO         : PAGE_READONLY;

        case RTMEM_PROT_WRITE:
        case RTMEM_PROT_WRITE | RTMEM_PROT_READ:
            return fKernel ? PAGE_KERNEL            : PAGE_SHARED;

        case RTMEM_PROT_EXEC:
        case RTMEM_PROT_EXEC | RTMEM_PROT_READ:
#if defined(RT_ARCH_X86) || defined(RT_ARCH_AMD64)
            if (fKernel)
            {
                pgprot_t fPg = MY_PAGE_KERNEL_EXEC;
                pgprot_val(fPg) &= ~_PAGE_RW;
                return fPg;
            }
            return PAGE_READONLY_EXEC;
#else
            return fKernel ? MY_PAGE_KERNEL_EXEC    : PAGE_READONLY_EXEC;
#endif

        case RTMEM_PROT_WRITE | RTMEM_PROT_EXEC:
        case RTMEM_PROT_WRITE | RTMEM_PROT_EXEC | RTMEM_PROT_READ:
            return fKernel ? MY_PAGE_KERNEL_EXEC    : PAGE_SHARED_EXEC;
    }
}


/**
 * Internal worker that allocates physical pages and creates the memory object for them.
 *
 * @returns IPRT status code.
 * @param   ppMemLnx    Where to store the memory object pointer.
 * @param   enmType     The object type.
 * @param   cb          The number of bytes to allocate.
 * @param   fFlagsLnx   The page allocation flags (GPFs).
 * @param   fContiguous Whether the allocation must be contiguous.
 */
static int rtR0MemObjLinuxAllocPages(PRTR0MEMOBJLNX *ppMemLnx, RTR0MEMOBJTYPE enmType, size_t cb, unsigned fFlagsLnx, bool fContiguous)
{
    size_t          iPage;
    size_t          cPages = cb >> PAGE_SHIFT;
    struct page    *paPages;

    /*
     * Allocate a memory object structure that's large enough to contain
     * the page pointer array.
     */
    PRTR0MEMOBJLNX  pMemLnx = (PRTR0MEMOBJLNX)rtR0MemObjNew(RT_OFFSETOF(RTR0MEMOBJLNX, apPages[cPages]), enmType, NULL, cb);
    if (!pMemLnx)
        return VERR_NO_MEMORY;
    pMemLnx->cPages = cPages;

    /*
     * Allocate the pages.
     * For small allocations we'll try contiguous first and then fall back on page by page.
     */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 22)
    if (    fContiguous
        ||  cb <= PAGE_SIZE * 2)
    {
        paPages = alloc_pages(fFlagsLnx, rtR0MemObjLinuxOrder(cb >> PAGE_SHIFT));
        if (paPages)
        {
            fContiguous = true;
            for (iPage = 0; iPage < cPages; iPage++)
                pMemLnx->apPages[iPage] = &paPages[iPage];
        }
        else if (fContiguous)
        {
            rtR0MemObjDelete(&pMemLnx->Core);
            return VERR_NO_MEMORY;
        }
    }

    if (!fContiguous)
    {
        for (iPage = 0; iPage < cPages; iPage++)
        {
            pMemLnx->apPages[iPage] = alloc_page(fFlagsLnx);
            if (RT_UNLIKELY(!pMemLnx->apPages[iPage]))
            {
                while (iPage-- > 0)
                    __free_page(pMemLnx->apPages[iPage]);
                rtR0MemObjDelete(&pMemLnx->Core);
                return VERR_NO_MEMORY;
            }
        }
    }

#else /* < 2.4.22 */
    /** @todo figure out why we didn't allocate page-by-page on 2.4.21 and older... */
    paPages = alloc_pages(fFlagsLnx, rtR0MemObjLinuxOrder(cb >> PAGE_SHIFT));
    if (!paPages)
    {
        rtR0MemObjDelete(&pMemLnx->Core);
        return VERR_NO_MEMORY;
    }
    for (iPage = 0; iPage < cPages; iPage++)
    {
        pMemLnx->apPages[iPage] = &paPages[iPage];
        MY_SET_PAGES_EXEC(pMemLnx->apPages[iPage], 1);
        if (PageHighMem(pMemLnx->apPages[iPage]))
            BUG();
    }

    fContiguous = true;
#endif /* < 2.4.22 */
    pMemLnx->fContiguous = fContiguous;

    /*
     * Reserve the pages.
     */
    for (iPage = 0; iPage < cPages; iPage++)
        SetPageReserved(pMemLnx->apPages[iPage]);

    *ppMemLnx = pMemLnx;
    return VINF_SUCCESS;
}


/**
 * Frees the physical pages allocated by the rtR0MemObjLinuxAllocPages() call.
 *
 * This method does NOT free the object.
 *
 * @param   pMemLnx     The object which physical pages should be freed.
 */
static void rtR0MemObjLinuxFreePages(PRTR0MEMOBJLNX pMemLnx)
{
    size_t iPage = pMemLnx->cPages;
    if (iPage > 0)
    {
        /*
         * Restore the page flags.
         */
        while (iPage-- > 0)
        {
            ClearPageReserved(pMemLnx->apPages[iPage]);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 22)
#else
            MY_SET_PAGES_NOEXEC(pMemLnx->apPages[iPage]);
#endif
        }

        /*
         * Free the pages.
         */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 22)
        if (!pMemLnx->fContiguous)
        {
            iPage = pMemLnx->cPages;
            while (iPage-- > 0)
                __free_page(pMemLnx->apPages[iPage]);
        }
        else
#endif
            __free_pages(pMemLnx->apPages[0], rtR0MemObjLinuxOrder(pMemLnx->cPages));

        pMemLnx->cPages = 0;
    }
}


/**
 * Maps the allocation into ring-0.
 *
 * This will update the RTR0MEMOBJLNX::Core.pv and RTR0MEMOBJ::fMappedToRing0 members.
 *
 * Contiguous mappings that isn't in 'high' memory will already be mapped into kernel
 * space, so we'll use that mapping if possible. If execute access is required, we'll
 * play safe and do our own mapping.
 *
 * @returns IPRT status code.
 * @param   pMemLnx     The linux memory object to map.
 * @param   fExecutable Whether execute access is required.
 */
static int rtR0MemObjLinuxVMap(PRTR0MEMOBJLNX pMemLnx, bool fExecutable)
{
    int rc = VINF_SUCCESS;

    /*
     * Choose mapping strategy.
     */
    bool fMustMap = fExecutable
                 || !pMemLnx->fContiguous;
    if (!fMustMap)
    {
        size_t iPage = pMemLnx->cPages;
        while (iPage-- > 0)
            if (PageHighMem(pMemLnx->apPages[iPage]))
            {
                fMustMap = true;
                break;
            }
    }

    Assert(!pMemLnx->Core.pv);
    Assert(!pMemLnx->fMappedToRing0);

    if (fMustMap)
    {
        /*
         * Use vmap - 2.4.22 and later.
         */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 22)
        pgprot_t fPg;
        pgprot_val(fPg) = _PAGE_PRESENT | _PAGE_RW;
# ifdef _PAGE_NX
        if (!fExecutable)
            pgprot_val(fPg) |= _PAGE_NX;
# endif

# ifdef VM_MAP
        pMemLnx->Core.pv = vmap(&pMemLnx->apPages[0], pMemLnx->cPages, VM_MAP, fPg);
# else
        pMemLnx->Core.pv = vmap(&pMemLnx->apPages[0], pMemLnx->cPages, VM_ALLOC, fPg);
# endif
        if (pMemLnx->Core.pv)
            pMemLnx->fMappedToRing0 = true;
        else
            rc = VERR_MAP_FAILED;
#else   /* < 2.4.22 */
        rc = VERR_NOT_SUPPORTED;
#endif
    }
    else
    {
        /*
         * Use the kernel RAM mapping.
         */
        pMemLnx->Core.pv = phys_to_virt(page_to_phys(pMemLnx->apPages[0]));
        Assert(pMemLnx->Core.pv);
    }

    return rc;
}


/**
 * Undos what rtR0MemObjLinuxVMap() did.
 *
 * @param   pMemLnx     The linux memory object.
 */
static void rtR0MemObjLinuxVUnmap(PRTR0MEMOBJLNX pMemLnx)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 22)
    if (pMemLnx->fMappedToRing0)
    {
        Assert(pMemLnx->Core.pv);
        vunmap(pMemLnx->Core.pv);
        pMemLnx->fMappedToRing0 = false;
    }
#else /* < 2.4.22 */
    Assert(!pMemLnx->fMappedToRing0);
#endif
    pMemLnx->Core.pv = NULL;
}


int rtR0MemObjNativeFree(RTR0MEMOBJ pMem)
{
    PRTR0MEMOBJLNX pMemLnx = (PRTR0MEMOBJLNX)pMem;

    /*
     * Release any memory that we've allocated or locked.
     */
    switch (pMemLnx->Core.enmType)
    {
        case RTR0MEMOBJTYPE_LOW:
        case RTR0MEMOBJTYPE_PAGE:
        case RTR0MEMOBJTYPE_CONT:
        case RTR0MEMOBJTYPE_PHYS:
        case RTR0MEMOBJTYPE_PHYS_NC:
            rtR0MemObjLinuxVUnmap(pMemLnx);
            rtR0MemObjLinuxFreePages(pMemLnx);
            break;

        case RTR0MEMOBJTYPE_LOCK:
            if (pMemLnx->Core.u.Lock.R0Process != NIL_RTR0PROCESS)
            {
                size_t iPage;
                struct task_struct *pTask = rtR0ProcessToLinuxTask(pMemLnx->Core.u.Lock.R0Process);
                Assert(pTask);
                if (pTask && pTask->mm)
                    down_read(&pTask->mm->mmap_sem);

                iPage = pMemLnx->cPages;
                while (iPage-- > 0)
                {
                    if (!PageReserved(pMemLnx->apPages[iPage]))
                        SetPageDirty(pMemLnx->apPages[iPage]);
                    page_cache_release(pMemLnx->apPages[iPage]);
                }

                if (pTask && pTask->mm)
                    up_read(&pTask->mm->mmap_sem);
            }
            else
                AssertFailed(); /* not implemented for R0 */
            break;

        case RTR0MEMOBJTYPE_RES_VIRT:
            Assert(pMemLnx->Core.pv);
            if (pMemLnx->Core.u.ResVirt.R0Process != NIL_RTR0PROCESS)
            {
                struct task_struct *pTask = rtR0ProcessToLinuxTask(pMemLnx->Core.u.Lock.R0Process);
                Assert(pTask);
                if (pTask && pTask->mm)
                {
                    down_write(&pTask->mm->mmap_sem);
                    MY_DO_MUNMAP(pTask->mm, (unsigned long)pMemLnx->Core.pv, pMemLnx->Core.cb);
                    up_write(&pTask->mm->mmap_sem);
                }
            }
            else
            {
                vunmap(pMemLnx->Core.pv);

                Assert(pMemLnx->cPages == 1 && pMemLnx->apPages[0] != NULL);
                __free_page(pMemLnx->apPages[0]);
                pMemLnx->apPages[0] = NULL;
                pMemLnx->cPages = 0;
            }
            pMemLnx->Core.pv = NULL;
            break;

        case RTR0MEMOBJTYPE_MAPPING:
            Assert(pMemLnx->cPages == 0); Assert(pMemLnx->Core.pv);
            if (pMemLnx->Core.u.ResVirt.R0Process != NIL_RTR0PROCESS)
            {
                struct task_struct *pTask = rtR0ProcessToLinuxTask(pMemLnx->Core.u.Lock.R0Process);
                Assert(pTask);
                if (pTask && pTask->mm)
                {
                    down_write(&pTask->mm->mmap_sem);
                    MY_DO_MUNMAP(pTask->mm, (unsigned long)pMemLnx->Core.pv, pMemLnx->Core.cb);
                    up_write(&pTask->mm->mmap_sem);
                }
            }
            else
                vunmap(pMemLnx->Core.pv);
            pMemLnx->Core.pv = NULL;
            break;

        default:
            AssertMsgFailed(("enmType=%d\n", pMemLnx->Core.enmType));
            return VERR_INTERNAL_ERROR;
    }
    return VINF_SUCCESS;
}


int rtR0MemObjNativeAllocPage(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    PRTR0MEMOBJLNX pMemLnx;
    int rc;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 22)
    rc = rtR0MemObjLinuxAllocPages(&pMemLnx, RTR0MEMOBJTYPE_PAGE, cb, GFP_HIGHUSER, false /* non-contiguous */);
#else
    rc = rtR0MemObjLinuxAllocPages(&pMemLnx, RTR0MEMOBJTYPE_PAGE, cb, GFP_USER, false /* non-contiguous */);
#endif
    if (RT_SUCCESS(rc))
    {
        rc = rtR0MemObjLinuxVMap(pMemLnx, fExecutable);
        if (RT_SUCCESS(rc))
        {
            *ppMem = &pMemLnx->Core;
            return rc;
        }

        rtR0MemObjLinuxFreePages(pMemLnx);
        rtR0MemObjDelete(&pMemLnx->Core);
    }

    return rc;
}


int rtR0MemObjNativeAllocLow(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    PRTR0MEMOBJLNX pMemLnx;
    int rc;

#ifdef RT_ARCH_AMD64
# ifdef GFP_DMA32
    rc = rtR0MemObjLinuxAllocPages(&pMemLnx, RTR0MEMOBJTYPE_LOW, cb, GFP_DMA32, false /* non-contiguous */);
    if (RT_FAILURE(rc))
# endif
        rc = rtR0MemObjLinuxAllocPages(&pMemLnx, RTR0MEMOBJTYPE_LOW, cb, GFP_DMA, false /* non-contiguous */);
#else
    rc = rtR0MemObjLinuxAllocPages(&pMemLnx, RTR0MEMOBJTYPE_LOW, cb, GFP_USER, false /* non-contiguous */);
#endif
    if (RT_SUCCESS(rc))
    {
        rc = rtR0MemObjLinuxVMap(pMemLnx, fExecutable);
        if (RT_SUCCESS(rc))
        {
            *ppMem = &pMemLnx->Core;
            return rc;
        }

        rtR0MemObjLinuxFreePages(pMemLnx);
        rtR0MemObjDelete(&pMemLnx->Core);
    }

    return rc;
}


int rtR0MemObjNativeAllocCont(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    PRTR0MEMOBJLNX pMemLnx;
    int rc;

#ifdef RT_ARCH_AMD64
# ifdef GFP_DMA32
    rc = rtR0MemObjLinuxAllocPages(&pMemLnx, RTR0MEMOBJTYPE_CONT, cb, GFP_DMA32, true /* contiguous */);
    if (RT_FAILURE(rc))
# endif
        rc = rtR0MemObjLinuxAllocPages(&pMemLnx, RTR0MEMOBJTYPE_CONT, cb, GFP_DMA, true /* contiguous */);
#else
    rc = rtR0MemObjLinuxAllocPages(&pMemLnx, RTR0MEMOBJTYPE_CONT, cb, GFP_USER, true /* contiguous */);
#endif
    if (RT_SUCCESS(rc))
    {
        rc = rtR0MemObjLinuxVMap(pMemLnx, fExecutable);
        if (RT_SUCCESS(rc))
        {
#ifdef RT_STRICT
            size_t iPage = pMemLnx->cPages;
            while (iPage-- > 0)
                Assert(page_to_phys(pMemLnx->apPages[iPage]) < _4G);
#endif
            pMemLnx->Core.u.Cont.Phys = page_to_phys(pMemLnx->apPages[0]);
            *ppMem = &pMemLnx->Core;
            return rc;
        }

        rtR0MemObjLinuxFreePages(pMemLnx);
        rtR0MemObjDelete(&pMemLnx->Core);
    }

    return rc;
}


/**
 * Worker for rtR0MemObjLinuxAllocPhysSub that tries one allocation strategy.
 *
 * @returns IPRT status.
 * @param   ppMemLnx    Where to
 * @param   enmType     The object type.
 * @param   cb          The size of the allocation.
 * @param   PhysHighest See rtR0MemObjNativeAllocPhys.
 * @param   fGfp        The Linux GFP flags to use for the allocation.
 */
static int rtR0MemObjLinuxAllocPhysSub2(PPRTR0MEMOBJINTERNAL ppMem, RTR0MEMOBJTYPE enmType, size_t cb, RTHCPHYS PhysHighest, unsigned fGfp)
{
    PRTR0MEMOBJLNX pMemLnx;
    int rc;

    rc = rtR0MemObjLinuxAllocPages(&pMemLnx, enmType, cb, fGfp,
                                   enmType == RTR0MEMOBJTYPE_PHYS /* contiguous / non-contiguous */);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Check the addresses if necessary. (Can be optimized a bit for PHYS.)
     */
    if (PhysHighest != NIL_RTHCPHYS)
    {
        size_t iPage = pMemLnx->cPages;
        while (iPage-- > 0)
            if (page_to_phys(pMemLnx->apPages[iPage]) >= PhysHighest)
            {
                rtR0MemObjLinuxFreePages(pMemLnx);
                rtR0MemObjDelete(&pMemLnx->Core);
                return VERR_NO_MEMORY;
            }
    }

    /*
     * Complete the object.
     */
    if (enmType == RTR0MEMOBJTYPE_PHYS)
    {
        pMemLnx->Core.u.Phys.PhysBase = page_to_phys(pMemLnx->apPages[0]);
        pMemLnx->Core.u.Phys.fAllocated = true;
    }
    *ppMem = &pMemLnx->Core;
    return rc;
}


/**
 * Worker for rtR0MemObjNativeAllocPhys and rtR0MemObjNativeAllocPhysNC.
 *
 * @returns IPRT status.
 * @param   ppMem       Where to store the memory object pointer on success.
 * @param   enmType     The object type.
 * @param   cb          The size of the allocation.
 * @param   PhysHighest See rtR0MemObjNativeAllocPhys.
 */
static int rtR0MemObjLinuxAllocPhysSub(PPRTR0MEMOBJINTERNAL ppMem, RTR0MEMOBJTYPE enmType, size_t cb, RTHCPHYS PhysHighest)
{
    int rc;

    /*
     * There are two clear cases and that's the <=16MB and anything-goes ones.
     * When the physical address limit is somewhere inbetween those two we'll
     * just have to try, starting with HIGHUSER and working our way thru the
     * different types, hoping we'll get lucky.
     *
     * We should probably move this physical address restriction logic up to
     * the page alloc function as it would be more efficient there. But since
     * we don't expect this to be a performance issue just yet it can wait.
     */
    if (PhysHighest == NIL_RTHCPHYS)
        rc = rtR0MemObjLinuxAllocPhysSub2(ppMem, enmType, cb, PhysHighest, GFP_HIGHUSER);
    else if (PhysHighest <= _1M * 16)
        rc = rtR0MemObjLinuxAllocPhysSub2(ppMem, enmType, cb, PhysHighest, GFP_DMA);
    else
    {
        rc = VERR_NO_MEMORY;
        if (RT_FAILURE(rc))
            rc = rtR0MemObjLinuxAllocPhysSub2(ppMem, enmType, cb, PhysHighest, GFP_HIGHUSER);
        if (RT_FAILURE(rc))
            rc = rtR0MemObjLinuxAllocPhysSub2(ppMem, enmType, cb, PhysHighest, GFP_USER);
#ifdef GFP_DMA32
        if (RT_FAILURE(rc))
            rc = rtR0MemObjLinuxAllocPhysSub2(ppMem, enmType, cb, PhysHighest, GFP_DMA32);
#endif
        if (RT_FAILURE(rc))
            rc = rtR0MemObjLinuxAllocPhysSub2(ppMem, enmType, cb, PhysHighest, GFP_DMA);
    }
    return rc;
}


int rtR0MemObjNativeAllocPhys(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, RTHCPHYS PhysHighest)
{
    return rtR0MemObjLinuxAllocPhysSub(ppMem, RTR0MEMOBJTYPE_PHYS, cb, PhysHighest);
}


int rtR0MemObjNativeAllocPhysNC(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, RTHCPHYS PhysHighest)
{
    return rtR0MemObjLinuxAllocPhysSub(ppMem, RTR0MEMOBJTYPE_PHYS_NC, cb, PhysHighest);
}


int rtR0MemObjNativeEnterPhys(PPRTR0MEMOBJINTERNAL ppMem, RTHCPHYS Phys, size_t cb)
{
    /*
     * All we need to do here is to validate that we can use
     * ioremap on the specified address (32/64-bit dma_addr_t).
     */
    PRTR0MEMOBJLNX  pMemLnx;
    dma_addr_t      PhysAddr = Phys;
    AssertMsgReturn(PhysAddr == Phys, ("%#llx\n", (unsigned long long)Phys), VERR_ADDRESS_TOO_BIG);

    pMemLnx = (PRTR0MEMOBJLNX)rtR0MemObjNew(sizeof(*pMemLnx), RTR0MEMOBJTYPE_PHYS, NULL, cb);
    if (!pMemLnx)
        return VERR_NO_MEMORY;

    pMemLnx->Core.u.Phys.PhysBase = PhysAddr;
    pMemLnx->Core.u.Phys.fAllocated = false;
    Assert(!pMemLnx->cPages);
    *ppMem = &pMemLnx->Core;
    return VINF_SUCCESS;
}


int rtR0MemObjNativeLockUser(PPRTR0MEMOBJINTERNAL ppMem, RTR3PTR R3Ptr, size_t cb, RTR0PROCESS R0Process)
{
    const int cPages = cb >> PAGE_SHIFT;
    struct task_struct *pTask = rtR0ProcessToLinuxTask(R0Process);
    struct vm_area_struct **papVMAs;
    PRTR0MEMOBJLNX pMemLnx;
    int rc = VERR_NO_MEMORY;

    /*
     * Check for valid task and size overflows.
     */
    if (!pTask)
        return VERR_NOT_SUPPORTED;
    if (((size_t)cPages << PAGE_SHIFT) != cb)
        return VERR_OUT_OF_RANGE;

    /*
     * Allocate the memory object and a temporary buffer for the VMAs.
     */
    pMemLnx = (PRTR0MEMOBJLNX)rtR0MemObjNew(RT_OFFSETOF(RTR0MEMOBJLNX, apPages[cPages]), RTR0MEMOBJTYPE_LOCK, (void *)R3Ptr, cb);
    if (!pMemLnx)
        return VERR_NO_MEMORY;

    papVMAs = (struct vm_area_struct **)RTMemAlloc(sizeof(*papVMAs) * cPages);
    if (papVMAs)
    {
        down_read(&pTask->mm->mmap_sem);

        /*
         * Get user pages.
         */
        rc = get_user_pages(pTask,                  /* Task for fault acounting. */
                            pTask->mm,              /* Whose pages. */
                            R3Ptr,                  /* Where from. */
                            cPages,                 /* How many pages. */
                            1,                      /* Write to memory. */
                            0,                      /* force. */
                            &pMemLnx->apPages[0],   /* Page array. */
                            papVMAs);               /* vmas */
        if (rc == cPages)
        {
            /*
             * Flush dcache (required?) and protect against fork.
             */
            /** @todo The Linux fork() protection will require more work if this API
             * is to be used for anything but locking VM pages. */
            while (rc-- > 0)
            {
                flush_dcache_page(pMemLnx->apPages[rc]);
                papVMAs[rc]->vm_flags |= VM_DONTCOPY;
            }

            up_read(&pTask->mm->mmap_sem);

            RTMemFree(papVMAs);

            pMemLnx->Core.u.Lock.R0Process = R0Process;
            pMemLnx->cPages = cPages;
            Assert(!pMemLnx->fMappedToRing0);
            *ppMem = &pMemLnx->Core;

            return VINF_SUCCESS;
        }

        /*
         * Failed - we need to unlock any pages that we succeeded to lock.
         */
        while (rc-- > 0)
        {
            if (!PageReserved(pMemLnx->apPages[rc]))
                SetPageDirty(pMemLnx->apPages[rc]);
            page_cache_release(pMemLnx->apPages[rc]);
        }

        up_read(&pTask->mm->mmap_sem);

        RTMemFree(papVMAs);
        rc = VERR_LOCK_FAILED;
    }

    rtR0MemObjDelete(&pMemLnx->Core);
    return rc;
}


int rtR0MemObjNativeLockKernel(PPRTR0MEMOBJINTERNAL ppMem, void *pv, size_t cb)
{
    /* What is there to lock? Should/Can we fake this? */
    return VERR_NOT_SUPPORTED;
}


int rtR0MemObjNativeReserveKernel(PPRTR0MEMOBJINTERNAL ppMem, void *pvFixed, size_t cb, size_t uAlignment)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 22)
    const size_t cPages = cb >> PAGE_SHIFT;
    struct page *pDummyPage;
    struct page **papPages;

    /* check for unsupported stuff. */
    AssertMsgReturn(pvFixed == (void *)-1, ("%p\n", pvFixed), VERR_NOT_SUPPORTED);
    AssertMsgReturn(uAlignment <= PAGE_SIZE, ("%#x\n", uAlignment), VERR_NOT_SUPPORTED);

    /*
     * Allocate a dummy page and create a page pointer array for vmap such that
     * the dummy page is mapped all over the reserved area.
     */
    pDummyPage = alloc_page(GFP_HIGHUSER);
    if (!pDummyPage)
        return VERR_NO_MEMORY;
    papPages = RTMemAlloc(sizeof(*papPages) * cPages);
    if (papPages)
    {
        void *pv;
        size_t iPage = cPages;
        while (iPage-- > 0)
            papPages[iPage] = pDummyPage;
# ifdef VM_MAP
        pv = vmap(papPages, cPages, VM_MAP, PAGE_KERNEL_RO);
# else
        pv = vmap(papPages, cPages, VM_ALLOC, PAGE_KERNEL_RO);
# endif
        RTMemFree(papPages);
        if (pv)
        {
            PRTR0MEMOBJLNX pMemLnx = (PRTR0MEMOBJLNX)rtR0MemObjNew(sizeof(*pMemLnx), RTR0MEMOBJTYPE_RES_VIRT, pv, cb);
            if (pMemLnx)
            {
                pMemLnx->Core.u.ResVirt.R0Process = NIL_RTR0PROCESS;
                pMemLnx->cPages = 1;
                pMemLnx->apPages[0] = pDummyPage;
                *ppMem = &pMemLnx->Core;
                return VINF_SUCCESS;
            }
            vunmap(pv);
        }
    }
    __free_page(pDummyPage);
    return VERR_NO_MEMORY;

#else   /* < 2.4.22 */
    /*
     * Could probably use ioremap here, but the caller is in a better position than us
     * to select some safe physical memory.
     */
    return VERR_NOT_SUPPORTED;
#endif
}


/**
 * Worker for rtR0MemObjNativeReserveUser and rtR0MemObjNativerMapUser that creates
 * an empty user space mapping.
 *
 * The caller takes care of acquiring the mmap_sem of the task.
 *
 * @returns Pointer to the mapping.
 *          (void *)-1 on failure.
 * @param   R3PtrFixed  (RTR3PTR)-1 if anywhere, otherwise a specific location.
 * @param   cb          The size of the mapping.
 * @param   uAlignment  The alignment of the mapping.
 * @param   pTask       The Linux task to create this mapping in.
 * @param   fProt       The RTMEM_PROT_* mask.
 */
static void *rtR0MemObjLinuxDoMmap(RTR3PTR R3PtrFixed, size_t cb, size_t uAlignment, struct task_struct *pTask, unsigned fProt)
{
    unsigned fLnxProt;
    unsigned long ulAddr;

    /*
     * Convert from IPRT protection to mman.h PROT_ and call do_mmap.
     */
    fProt &= (RTMEM_PROT_NONE | RTMEM_PROT_READ | RTMEM_PROT_WRITE | RTMEM_PROT_EXEC);
    if (fProt == RTMEM_PROT_NONE)
        fLnxProt = PROT_NONE;
    else
    {
        fLnxProt = 0;
        if (fProt & RTMEM_PROT_READ)
            fLnxProt |= PROT_READ;
        if (fProt & RTMEM_PROT_WRITE)
            fLnxProt |= PROT_WRITE;
        if (fProt & RTMEM_PROT_EXEC)
            fLnxProt |= PROT_EXEC;
    }

    if (R3PtrFixed != (RTR3PTR)-1)
        ulAddr = do_mmap(NULL, R3PtrFixed, cb, fLnxProt, MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED, 0);
    else
    {
        ulAddr = do_mmap(NULL, 0, cb, fLnxProt, MAP_SHARED | MAP_ANONYMOUS, 0);
        if (    !(ulAddr & ~PAGE_MASK)
            &&  (ulAddr & (uAlignment - 1)))
        {
            /** @todo implement uAlignment properly... We'll probably need to make some dummy mappings to fill
             * up alignment gaps. This is of course complicated by fragmentation (which we might have cause
             * ourselves) and further by there begin two mmap strategies (top / bottom). */
            /* For now, just ignore uAlignment requirements... */
        }
    }
    if (ulAddr & ~PAGE_MASK) /* ~PAGE_MASK == PAGE_OFFSET_MASK */
        return (void *)-1;
    return (void *)ulAddr;
}


int rtR0MemObjNativeReserveUser(PPRTR0MEMOBJINTERNAL ppMem, RTR3PTR R3PtrFixed, size_t cb, size_t uAlignment, RTR0PROCESS R0Process)
{
    PRTR0MEMOBJLNX      pMemLnx;
    void               *pv;
    struct task_struct *pTask = rtR0ProcessToLinuxTask(R0Process);
    if (!pTask)
        return VERR_NOT_SUPPORTED;

    /*
     * Let rtR0MemObjLinuxDoMmap do the difficult bits.
     */
    down_write(&pTask->mm->mmap_sem);
    pv = rtR0MemObjLinuxDoMmap(R3PtrFixed, cb, uAlignment, pTask, RTMEM_PROT_NONE);
    up_write(&pTask->mm->mmap_sem);
    if (pv == (void *)-1)
        return VERR_NO_MEMORY;

    pMemLnx = (PRTR0MEMOBJLNX)rtR0MemObjNew(sizeof(*pMemLnx), RTR0MEMOBJTYPE_RES_VIRT, pv, cb);
    if (!pMemLnx)
    {
        down_write(&pTask->mm->mmap_sem);
        MY_DO_MUNMAP(pTask->mm, (unsigned long)pv, cb);
        up_write(&pTask->mm->mmap_sem);
        return VERR_NO_MEMORY;
    }

    pMemLnx->Core.u.ResVirt.R0Process = R0Process;
    *ppMem = &pMemLnx->Core;
    return VINF_SUCCESS;
}


int rtR0MemObjNativeMapKernel(PPRTR0MEMOBJINTERNAL ppMem, RTR0MEMOBJ pMemToMap, void *pvFixed, size_t uAlignment, unsigned fProt)
{
    int rc = VERR_NO_MEMORY;
    PRTR0MEMOBJLNX pMemLnxToMap = (PRTR0MEMOBJLNX)pMemToMap;
    PRTR0MEMOBJLNX pMemLnx;

    /* Fail if requested to do something we can't. */
    AssertMsgReturn(pvFixed == (void *)-1, ("%p\n", pvFixed), VERR_NOT_SUPPORTED);
    AssertMsgReturn(uAlignment <= PAGE_SIZE, ("%#x\n", uAlignment), VERR_NOT_SUPPORTED);

    /*
     * Create the IPRT memory object.
     */
    pMemLnx = (PRTR0MEMOBJLNX)rtR0MemObjNew(sizeof(*pMemLnx), RTR0MEMOBJTYPE_MAPPING, NULL, pMemLnxToMap->Core.cb);
    if (pMemLnx)
    {
        if (pMemLnxToMap->cPages)
        {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 22)
            /*
             * Use vmap - 2.4.22 and later.
             */
            pgprot_t fPg = rtR0MemObjLinuxConvertProt(fProt, true /* kernel */);
# ifdef VM_MAP
            pMemLnx->Core.pv = vmap(&pMemLnxToMap->apPages[0], pMemLnxToMap->cPages, VM_MAP, fPg);
# else
            pMemLnx->Core.pv = vmap(&pMemLnxToMap->apPages[0], pMemLnxToMap->cPages, VM_ALLOC, fPg);
# endif
            if (pMemLnx->Core.pv)
            {
                pMemLnx->fMappedToRing0 = true;
                rc = VINF_SUCCESS;
            }
            else
                rc = VERR_MAP_FAILED;

#else   /* < 2.4.22 */
            /*
             * Only option here is to share mappings if possible and forget about fProt.
             */
            if (rtR0MemObjIsRing3(pMemToMap))
                rc = VERR_NOT_SUPPORTED;
            else
            {
                rc = VINF_SUCCESS;
                if (!pMemLnxToMap->Core.pv)
                    rc = rtR0MemObjLinuxVMap(pMemLnxToMap, !!(fProt & RTMEM_PROT_EXEC));
                if (RT_SUCCESS(rc))
                {
                    Assert(pMemLnxToMap->Core.pv);
                    pMemLnx->Core.pv = pMemLnxToMap->Core.pv;
                }
            }
#endif
        }
        else
        {
            /*
             * MMIO / physical memory.
             */
            Assert(pMemLnxToMap->Core.enmType == RTR0MEMOBJTYPE_PHYS && !pMemLnxToMap->Core.u.Phys.fAllocated);
            pMemLnx->Core.pv = ioremap(pMemLnxToMap->Core.u.Phys.PhysBase, pMemLnxToMap->Core.cb);
            if (pMemLnx->Core.pv)
            {
                /** @todo fix protection. */
                rc = VINF_SUCCESS;
            }
        }
        if (RT_SUCCESS(rc))
        {
            pMemLnx->Core.u.Mapping.R0Process = NIL_RTR0PROCESS;
            *ppMem = &pMemLnx->Core;
            return VINF_SUCCESS;
        }
        rtR0MemObjDelete(&pMemLnx->Core);
    }

    return rc;
}


int rtR0MemObjNativeMapUser(PPRTR0MEMOBJINTERNAL ppMem, RTR0MEMOBJ pMemToMap, RTR3PTR R3PtrFixed, size_t uAlignment, unsigned fProt, RTR0PROCESS R0Process)
{
    struct task_struct *pTask        = rtR0ProcessToLinuxTask(R0Process);
    PRTR0MEMOBJLNX      pMemLnxToMap = (PRTR0MEMOBJLNX)pMemToMap;
    int                 rc           = VERR_NO_MEMORY;
    PRTR0MEMOBJLNX      pMemLnx;

    /*
     * Check for restrictions.
     */
    if (!pTask)
        return VERR_NOT_SUPPORTED;

    /*
     * Create the IPRT memory object.
     */
    pMemLnx = (PRTR0MEMOBJLNX)rtR0MemObjNew(sizeof(*pMemLnx), RTR0MEMOBJTYPE_MAPPING, NULL, pMemLnxToMap->Core.cb);
    if (pMemLnx)
    {
        /*
         * Allocate user space mapping.
         */
        void *pv;
        down_write(&pTask->mm->mmap_sem);
        pv = rtR0MemObjLinuxDoMmap(R3PtrFixed, pMemLnxToMap->Core.cb, uAlignment, pTask, fProt);
        if (pv != (void *)-1)
        {
            /*
             * Map page by page into the mmap area.
             * This is generic, paranoid and not very efficient.
             */
            pgprot_t        fPg = rtR0MemObjLinuxConvertProt(fProt, false /* user */);
            unsigned long   ulAddrCur = (unsigned long)pv;
            const size_t    cPages = pMemLnxToMap->Core.cb >> PAGE_SHIFT;
            size_t          iPage;
            rc = 0;
            if (pMemLnxToMap->cPages)
            {
                for (iPage = 0; iPage < cPages; iPage++, ulAddrCur += PAGE_SIZE)
                {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) || defined(HAVE_26_STYLE_REMAP_PAGE_RANGE)
                    struct vm_area_struct *vma = find_vma(pTask->mm, ulAddrCur); /* this is probably the same for all the pages... */
                    AssertBreak(vma, rc = VERR_INTERNAL_ERROR);
#endif

#if   LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 11)
                    rc = remap_pfn_range(vma, ulAddrCur, page_to_pfn(pMemLnxToMap->apPages[iPage]), PAGE_SIZE, fPg);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) || defined(HAVE_26_STYLE_REMAP_PAGE_RANGE)
                    rc = remap_page_range(vma, ulAddrCur, page_to_phys(pMemLnxToMap->apPages[iPage]), PAGE_SIZE, fPg);
#else /* 2.4 */
                    rc = remap_page_range(ulAddrCur, page_to_phys(pMemLnxToMap->apPages[iPage]), PAGE_SIZE, fPg);
#endif
                    if (rc)
                        break;
                }
            }
            else
            {
                RTHCPHYS Phys;
                if (pMemLnxToMap->Core.enmType == RTR0MEMOBJTYPE_PHYS)
                    Phys = pMemLnxToMap->Core.u.Phys.PhysBase;
                else if (pMemLnxToMap->Core.enmType == RTR0MEMOBJTYPE_CONT)
                    Phys = pMemLnxToMap->Core.u.Cont.Phys;
                else
                {
                    AssertMsgFailed(("%d\n", pMemLnxToMap->Core.enmType));
                    Phys = NIL_RTHCPHYS;
                }
                if (Phys != NIL_RTHCPHYS)
                {
                    for (iPage = 0; iPage < cPages; iPage++, ulAddrCur += PAGE_SIZE, Phys += PAGE_SIZE)
                    {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) || defined(HAVE_26_STYLE_REMAP_PAGE_RANGE)
                        struct vm_area_struct *vma = find_vma(pTask->mm, ulAddrCur); /* this is probably the same for all the pages... */
                        AssertBreak(vma, rc = VERR_INTERNAL_ERROR);
#endif

#if   LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 11)
                        rc = remap_pfn_range(vma, ulAddrCur, Phys, PAGE_SIZE, fPg);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) || defined(HAVE_26_STYLE_REMAP_PAGE_RANGE)
                        rc = remap_page_range(vma, ulAddrCur, Phys, PAGE_SIZE, fPg);
#else /* 2.4 */
                        rc = remap_page_range(ulAddrCur, Phys, PAGE_SIZE, fPg);
#endif
                        if (rc)
                            break;
                    }
                }
            }
            if (!rc)
            {
                up_write(&pTask->mm->mmap_sem);

                pMemLnx->Core.pv = pv;
                pMemLnx->Core.u.Mapping.R0Process = R0Process;
                *ppMem = &pMemLnx->Core;
                return VINF_SUCCESS;
            }

            /*
             * Bail out.
             */
            MY_DO_MUNMAP(pTask->mm, (unsigned long)pv, pMemLnxToMap->Core.cb);
            if (rc != VERR_INTERNAL_ERROR)
                rc = VERR_NO_MEMORY;
        }

        up_write(&pTask->mm->mmap_sem);

        rtR0MemObjDelete(&pMemLnx->Core);
    }

    return rc;
}


RTHCPHYS rtR0MemObjNativeGetPagePhysAddr(PRTR0MEMOBJINTERNAL pMem, size_t iPage)
{
    PRTR0MEMOBJLNX  pMemLnx = (PRTR0MEMOBJLNX)pMem;

    if (pMemLnx->cPages)
        return page_to_phys(pMemLnx->apPages[iPage]);

    switch (pMemLnx->Core.enmType)
    {
        case RTR0MEMOBJTYPE_CONT:
            return pMemLnx->Core.u.Cont.Phys     + (iPage << PAGE_SHIFT);

        case RTR0MEMOBJTYPE_PHYS:
            return pMemLnx->Core.u.Phys.PhysBase + (iPage << PAGE_SHIFT);

            /* the parent knows */
        case RTR0MEMOBJTYPE_MAPPING:
            return rtR0MemObjNativeGetPagePhysAddr(pMemLnx->Core.uRel.Child.pParent, iPage);

            /* cPages > 0 */
        case RTR0MEMOBJTYPE_LOW:
        case RTR0MEMOBJTYPE_LOCK:
        case RTR0MEMOBJTYPE_PHYS_NC:
        case RTR0MEMOBJTYPE_PAGE:
        default:
            AssertMsgFailed(("%d\n", pMemLnx->Core.enmType));
            /* fall thru */

        case RTR0MEMOBJTYPE_RES_VIRT:
            return NIL_RTHCPHYS;
    }
}

