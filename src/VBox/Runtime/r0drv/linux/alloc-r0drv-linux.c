/* $Id$ */
/** @file
 * IPRT - Memory Allocation, Ring-0 Driver, Linux.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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
#include "internal/iprt.h"
#include <iprt/mem.h>

#include <iprt/assert.h>
#include <iprt/err.h>
#include "r0drv/alloc-r0drv.h"

#if defined(RT_ARCH_AMD64) || defined(DOXYGEN_RUNNING)
/**
 * We need memory in the module range (~2GB to ~0) this can only be obtained
 * thru APIs that are not exported (see module_alloc()).
 *
 * So, we'll have to create a quick and dirty heap here using BSS memory.
 * Very annoying and it's going to restrict us!
 */
# define RTMEMALLOC_EXEC_HEAP
#endif
#ifdef RTMEMALLOC_EXEC_HEAP
# include <iprt/heap.h>
# include <iprt/spinlock.h>
# include <iprt/err.h>
#endif


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
#ifdef RTMEMALLOC_EXEC_HEAP

# ifdef CONFIG_DEBUG_SET_MODULE_RONX
#  define RTMEMALLOC_EXEC_HEAP_VM_AREA  1
# endif
/** The heap. */
static RTHEAPSIMPLE g_HeapExec = NIL_RTHEAPSIMPLE;
/** Spinlock protecting the heap. */
static RTSPINLOCK   g_HeapExecSpinlock = NIL_RTSPINLOCK;
# ifdef RTMEMALLOC_EXEC_HEAP_VM_AREA
static struct page **g_apPages;
static void *g_pvHeap;
static size_t g_cPages;
# endif


/**
 * API for cleaning up the heap spinlock on IPRT termination.
 * This is as RTMemExecDonate specific to AMD64 Linux/GNU.
 */
DECLHIDDEN(void) rtR0MemExecCleanup(void)
{
# ifdef RTMEMALLOC_EXEC_HEAP_VM_AREA
    unsigned i;

    /* according to linux/drivers/lguest/core.c this function undoes
     * map_vm_area() as well as __get_vm_area(). */
    if (g_pvHeap)
        vunmap(g_pvHeap);
    for (i = 0; i < g_cPages; i++)
        __free_page(g_apPages[i]);
    kfree(g_apPages);
# endif

    RTSpinlockDestroy(g_HeapExecSpinlock);
    g_HeapExecSpinlock = NIL_RTSPINLOCK;
}


# ifndef RTMEMALLOC_EXEC_HEAP_VM_AREA
/**
 * Donate read+write+execute memory to the exec heap.
 *
 * This API is specific to AMD64 and Linux/GNU. A kernel module that desires to
 * use RTMemExecAlloc on AMD64 Linux/GNU will have to donate some statically
 * allocated memory in the module if it wishes for GCC generated code to work.
 * GCC can only generate modules that work in the address range ~2GB to ~0
 * currently.
 *
 * The API only accept one single donation.
 *
 * @returns IPRT status code.
 * @param   pvMemory    Pointer to the memory block.
 * @param   cb          The size of the memory block.
 */
RTR0DECL(int) RTR0MemExecDonate(void *pvMemory, size_t cb)
{
    int rc;
    AssertReturn(g_HeapExec == NIL_RTHEAPSIMPLE, VERR_WRONG_ORDER);

    rc = RTSpinlockCreate(&g_HeapExecSpinlock, RTSPINLOCK_FLAGS_INTERRUPT_SAFE, "RTR0MemExecDonate");
    if (RT_SUCCESS(rc))
    {
        rc = RTHeapSimpleInit(&g_HeapExec, pvMemory, cb);
        if (RT_FAILURE(rc))
            rtR0MemExecCleanup();
    }
    return rc;
}
RT_EXPORT_SYMBOL(RTR0MemExecDonate);

# else /* !RTMEMALLOC_EXEC_HEAP_VM_AREA */

/**
 * RTR0MemExecDonate() does not work if CONFIG_DEBUG_SET_MODULE_RONX is enabled.
 * In that case, allocate a VM area in the modules range and back it with kernel
 * memory. Unfortunately __vmalloc_area() is not exported so we have to emulate
 * it.
 */
RTR0DECL(int) RTR0MemExecInit(size_t cb)
{
    int rc;
    struct vm_struct *area;
    size_t cPages;
    size_t cbPages;
    unsigned i;
    struct page **ppPages;

    AssertReturn(g_HeapExec == NIL_RTHEAPSIMPLE, VERR_WRONG_ORDER);

    rc = RTSpinlockCreate(&g_HeapExecSpinlock, RTSPINLOCK_FLAGS_INTERRUPT_SAFE, "RTR0MemExecInit");
    if (RT_SUCCESS(rc))
    {
        cb = RT_ALIGN(cb, PAGE_SIZE);
        area = __get_vm_area(cb, VM_ALLOC, MODULES_VADDR, MODULES_END);
        if (!area)
        {
            rtR0MemExecCleanup();
            return VERR_NO_MEMORY;
        }
        g_pvHeap = area->addr;
        cPages = cb >> PAGE_SHIFT;
        area->nr_pages = 0;
        cbPages = cPages * sizeof(struct page *);
        g_apPages = kmalloc(cbPages, GFP_KERNEL);
        area->pages = g_apPages;
        if (!g_apPages)
        {
            rtR0MemExecCleanup();
            return VERR_NO_MEMORY;
        }
        memset(area->pages, 0, cbPages);
        for (i = 0; i < cPages; i++)
        {
            g_apPages[i] = alloc_page(GFP_KERNEL | __GFP_HIGHMEM);
            if (!g_apPages[i])
            {
                area->nr_pages = i;
                g_cPages = i;
                rtR0MemExecCleanup();
                return VERR_NO_MEMORY;
            }
        }
        area->nr_pages = cPages;
        g_cPages = i;
        ppPages = g_apPages;
        if (map_vm_area(area, PAGE_KERNEL_EXEC, &ppPages))
        {
            rtR0MemExecCleanup();
            return VERR_NO_MEMORY;
        }

        rc = RTHeapSimpleInit(&g_HeapExec, g_pvHeap, cb);
        if (RT_FAILURE(rc))
            rtR0MemExecCleanup();
    }
    return rc;
}
RT_EXPORT_SYMBOL(RTR0MemExecInit);
# endif /* RTMEMALLOC_EXEC_HEAP_VM_AREA */
#endif /* RTMEMALLOC_EXEC_HEAP */



/**
 * OS specific allocation function.
 */
DECLHIDDEN(int) rtR0MemAllocEx(size_t cb, uint32_t fFlags, PRTMEMHDR *ppHdr)
{
    PRTMEMHDR pHdr;

    /*
     * Allocate.
     */
    if (fFlags & RTMEMHDR_FLAG_EXEC)
    {
        if (fFlags & RTMEMHDR_FLAG_ANY_CTX)
            return VERR_NOT_SUPPORTED;

#if defined(RT_ARCH_AMD64)
# ifdef RTMEMALLOC_EXEC_HEAP
        if (g_HeapExec != NIL_RTHEAPSIMPLE)
        {
            RTSpinlockAcquire(g_HeapExecSpinlock);
            pHdr = (PRTMEMHDR)RTHeapSimpleAlloc(g_HeapExec, cb + sizeof(*pHdr), 0);
            RTSpinlockRelease(g_HeapExecSpinlock);
            fFlags |= RTMEMHDR_FLAG_EXEC_HEAP;
        }
        else
            pHdr = NULL;
# else  /* !RTMEMALLOC_EXEC_HEAP */
        pHdr = (PRTMEMHDR)__vmalloc(cb + sizeof(*pHdr), GFP_KERNEL | __GFP_HIGHMEM, MY_PAGE_KERNEL_EXEC);
# endif /* !RTMEMALLOC_EXEC_HEAP */

#elif defined(PAGE_KERNEL_EXEC) && defined(CONFIG_X86_PAE)
        pHdr = (PRTMEMHDR)__vmalloc(cb + sizeof(*pHdr), GFP_KERNEL | __GFP_HIGHMEM, MY_PAGE_KERNEL_EXEC);
#else
        pHdr = (PRTMEMHDR)vmalloc(cb + sizeof(*pHdr));
#endif
    }
    else
    {
        if (
#if 1 /* vmalloc has serious performance issues, avoid it. */
               cb <= PAGE_SIZE*16 - sizeof(*pHdr)
#else
               cb <= PAGE_SIZE
#endif
            || (fFlags & RTMEMHDR_FLAG_ANY_CTX)
           )
        {
            fFlags |= RTMEMHDR_FLAG_KMALLOC;
            pHdr = kmalloc(cb + sizeof(*pHdr),
                           (fFlags & RTMEMHDR_FLAG_ANY_CTX_ALLOC) ? GFP_ATOMIC : GFP_KERNEL);
            if (RT_UNLIKELY(   !pHdr
                            && cb > PAGE_SIZE
                            && !(fFlags & RTMEMHDR_FLAG_ANY_CTX) ))
            {
                fFlags &= ~RTMEMHDR_FLAG_KMALLOC;
                pHdr = vmalloc(cb + sizeof(*pHdr));
            }
        }
        else
            pHdr = vmalloc(cb + sizeof(*pHdr));
    }
    if (RT_UNLIKELY(!pHdr))
        return VERR_NO_MEMORY;

    /*
     * Initialize.
     */
    pHdr->u32Magic  = RTMEMHDR_MAGIC;
    pHdr->fFlags    = fFlags;
    pHdr->cb        = cb;
    pHdr->cbReq     = cb;

    *ppHdr = pHdr;
    return VINF_SUCCESS;
}


/**
 * OS specific free function.
 */
DECLHIDDEN(void) rtR0MemFree(PRTMEMHDR pHdr)
{
    pHdr->u32Magic += 1;
    if (pHdr->fFlags & RTMEMHDR_FLAG_KMALLOC)
        kfree(pHdr);
#ifdef RTMEMALLOC_EXEC_HEAP
    else if (pHdr->fFlags & RTMEMHDR_FLAG_EXEC_HEAP)
    {
        RTSpinlockAcquire(g_HeapExecSpinlock);
        RTHeapSimpleFree(g_HeapExec, pHdr);
        RTSpinlockRelease(g_HeapExecSpinlock);
    }
#endif
    else
        vfree(pHdr);
}


/**
 * Compute order. Some functions allocate 2^order pages.
 *
 * @returns order.
 * @param   cPages      Number of pages.
 */
static int CalcPowerOf2Order(unsigned long cPages)
{
    int             iOrder;
    unsigned long   cTmp;

    for (iOrder = 0, cTmp = cPages; cTmp >>= 1; ++iOrder)
        ;
    if (cPages & ~(1 << iOrder))
        ++iOrder;

    return iOrder;
}


/**
 * Allocates physical contiguous memory (below 4GB).
 * The allocation is page aligned and the content is undefined.
 *
 * @returns Pointer to the memory block. This is page aligned.
 * @param   pPhys   Where to store the physical address.
 * @param   cb      The allocation size in bytes. This is always
 *                  rounded up to PAGE_SIZE.
 */
RTR0DECL(void *) RTMemContAlloc(PRTCCPHYS pPhys, size_t cb)
{
    int             cOrder;
    unsigned        cPages;
    struct page    *paPages;

    /*
     * validate input.
     */
    Assert(VALID_PTR(pPhys));
    Assert(cb > 0);

    /*
     * Allocate page pointer array.
     */
    cb = RT_ALIGN_Z(cb, PAGE_SIZE);
    cPages = cb >> PAGE_SHIFT;
    cOrder = CalcPowerOf2Order(cPages);
#if (defined(RT_ARCH_AMD64) || defined(CONFIG_X86_PAE)) && defined(GFP_DMA32)
    /* ZONE_DMA32: 0-4GB */
    paPages = alloc_pages(GFP_DMA32, cOrder);
    if (!paPages)
#endif
#ifdef RT_ARCH_AMD64
        /* ZONE_DMA; 0-16MB */
        paPages = alloc_pages(GFP_DMA, cOrder);
#else
        /* ZONE_NORMAL: 0-896MB */
        paPages = alloc_pages(GFP_USER, cOrder);
#endif
    if (paPages)
    {
        /*
         * Reserve the pages and mark them executable.
         */
        unsigned iPage;
        for (iPage = 0; iPage < cPages; iPage++)
        {
            Assert(!PageHighMem(&paPages[iPage]));
            if (iPage + 1 < cPages)
            {
                AssertMsg(          (uintptr_t)phys_to_virt(page_to_phys(&paPages[iPage])) + PAGE_SIZE
                                ==  (uintptr_t)phys_to_virt(page_to_phys(&paPages[iPage + 1]))
                          &&        page_to_phys(&paPages[iPage]) + PAGE_SIZE
                                ==  page_to_phys(&paPages[iPage + 1]),
                          ("iPage=%i cPages=%u [0]=%#llx,%p [1]=%#llx,%p\n", iPage, cPages,
                           (long long)page_to_phys(&paPages[iPage]),     phys_to_virt(page_to_phys(&paPages[iPage])),
                           (long long)page_to_phys(&paPages[iPage + 1]), phys_to_virt(page_to_phys(&paPages[iPage + 1])) ));
            }

            SetPageReserved(&paPages[iPage]);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 4, 20) /** @todo find the exact kernel where change_page_attr was introduced. */
            MY_SET_PAGES_EXEC(&paPages[iPage], 1);
#endif
        }
        *pPhys = page_to_phys(paPages);
        return phys_to_virt(page_to_phys(paPages));
    }

    return NULL;
}
RT_EXPORT_SYMBOL(RTMemContAlloc);


/**
 * Frees memory allocated ysing RTMemContAlloc().
 *
 * @param   pv      Pointer to return from RTMemContAlloc().
 * @param   cb      The cb parameter passed to RTMemContAlloc().
 */
RTR0DECL(void) RTMemContFree(void *pv, size_t cb)
{
    if (pv)
    {
        int             cOrder;
        unsigned        cPages;
        unsigned        iPage;
        struct page    *paPages;

        /* validate */
        AssertMsg(!((uintptr_t)pv & PAGE_OFFSET_MASK), ("pv=%p\n", pv));
        Assert(cb > 0);

        /* calc order and get pages */
        cb = RT_ALIGN_Z(cb, PAGE_SIZE);
        cPages = cb >> PAGE_SHIFT;
        cOrder = CalcPowerOf2Order(cPages);
        paPages = virt_to_page(pv);

        /*
         * Restore page attributes freeing the pages.
         */
        for (iPage = 0; iPage < cPages; iPage++)
        {
            ClearPageReserved(&paPages[iPage]);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 4, 20) /** @todo find the exact kernel where change_page_attr was introduced. */
            MY_SET_PAGES_NOEXEC(&paPages[iPage], 1);
#endif
        }
        __free_pages(paPages, cOrder);
    }
}
RT_EXPORT_SYMBOL(RTMemContFree);

