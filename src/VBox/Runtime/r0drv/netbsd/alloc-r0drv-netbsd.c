/* $Id$ */
/** @file
 * IPRT - Memory Allocation, Ring-0 Driver, NetBSD.
 */

/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "the-netbsd-kernel.h"
#include "internal/iprt.h"
#include <iprt/mem.h>

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/param.h>

#include "r0drv/alloc-r0drv.h"


DECLHIDDEN(int) rtR0MemAllocEx(size_t cb, uint32_t fFlags, PRTMEMHDR *ppHdr)
{
    size_t      cbAllocated = cb;
    PRTMEMHDR   pHdr        = NULL;

    if (fFlags & RTMEMHDR_FLAG_ZEROED)
        pHdr = kmem_zalloc(cb + sizeof(RTMEMHDR), KM_NOSLEEP);
    else
        pHdr = kmem_alloc(cb + sizeof(RTMEMHDR), KM_NOSLEEP);

    if (RT_UNLIKELY(!pHdr))
        return VERR_NO_MEMORY;

    pHdr->u32Magic   = RTMEMHDR_MAGIC;
    pHdr->fFlags     = fFlags;
    pHdr->cb         = cbAllocated;
    pHdr->cbReq      = cb;

    *ppHdr = pHdr;
    return VINF_SUCCESS;
}


DECLHIDDEN(void) rtR0MemFree(PRTMEMHDR pHdr)
{
    pHdr->u32Magic += 1;

    kmem_free(pHdr, pHdr->cb + sizeof(RTMEMHDR));
}

RTR0DECL(void) RTMemContFree(void *pv, size_t cb)
{
    if (pv)
    {
        cb = round_page(cb);

        paddr_t pa;
        pmap_extract(pmap_kernel(), (vaddr_t)pv, &pa);

        /*
         * Reconstruct pglist to free the physical pages
         */
        struct pglist rlist;
        TAILQ_INIT(&rlist);

        for (paddr_t pa2 = pa ; pa2 < pa + cb ; pa2 += PAGE_SIZE) {
            struct vm_page *page = PHYS_TO_VM_PAGE(pa2);
            TAILQ_INSERT_TAIL(&rlist, page, pageq.queue);
        }

        /* Unmap */
        pmap_kremove((vaddr_t)pv, cb);

        /* Free the virtual space */
        uvm_km_free(kernel_map, (vaddr_t)pv, cb, UVM_KMF_VAONLY);

        /* Free the physical pages */
        uvm_pglistfree(&rlist);
    }
}

RTR0DECL(void *) RTMemContAlloc(PRTCCPHYS pPhys, size_t cb)
{
    /*
     * Validate input.
     */
    AssertPtr(pPhys);
    Assert(cb > 0);

    cb = round_page(cb);

    vaddr_t virt = uvm_km_alloc(kernel_map, cb, 0,
            UVM_KMF_VAONLY | UVM_KMF_WAITVA | UVM_KMF_CANFAIL);
    if (virt == 0)
        return NULL;

    struct pglist rlist;

    if (uvm_pglistalloc(cb, 0, (paddr_t)0xFFFFFFFF,
            PAGE_SIZE, 0, &rlist, 1, 1) != 0)
    {
        uvm_km_free(kernel_map, virt, cb, UVM_KMF_VAONLY);
        return NULL;
    }

    struct vm_page *page;
    vaddr_t virt2 = virt;
    TAILQ_FOREACH(page, &rlist, pageq.queue)
    {
        pmap_kenter_pa(virt2, VM_PAGE_TO_PHYS(page),
                VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE, 0);
        virt2 += PAGE_SIZE;
    }

    page = TAILQ_FIRST(&rlist);
    *pPhys = VM_PAGE_TO_PHYS(page);

    return (void *)virt;
}
