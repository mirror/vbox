/* $Id$ */
/** @file
 * Incredibly Portable Runtime - Ring-0 Memory Objects, Solaris.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-solaris-kernel.h"

#include <iprt/memobj.h>
#include <iprt/mem.h>
#include <iprt/err.h>
#include <iprt/assert.h>
#include <iprt/log.h>
#include <iprt/param.h>
#include <iprt/process.h>
#include "internal/memobj.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * The Solaris version of the memory object structure.
 */
typedef struct RTR0MEMOBJSOLARIS
{
    /** The core structure. */
    RTR0MEMOBJINTERNAL  Core;
    /** Pointer to kernel memory cookie. */
    ddi_umem_cookie_t   Cookie;
    /** Shadow locked pages. */
    page_t              **ppShadowPages;
} RTR0MEMOBJSOLARIS, *PRTR0MEMOBJSOLARIS;

/**
 * Used for supplying the solaris kernel info. about memory limits
 * during contiguous allocations (i_ddi_mem_alloc)
 */
struct ddi_dma_attr g_SolarisX86PhysMemLimits =
{
    DMA_ATTR_V0,            /* Version Number */
    (uint64_t)0,            /* lower limit */
    (uint64_t)0xffffffff,   /* high limit (32-bit PA, 4G) */
    (uint64_t)0xffffffff,   /* counter limit */
    (uint64_t)PAGE_SIZE,    /* alignment */
    (uint64_t)PAGE_SIZE,    /* burst size */
    (uint64_t)PAGE_SIZE,    /* effective DMA size */
    (uint64_t)0xffffffff,   /* max DMA xfer size */
    (uint64_t)0xffffffff,   /* segment boundary */
    1,                      /* scatter-gather list length (1 for contiguous) */
    1,                      /* device granularity */
    0                       /* bus-specific flags */
};



static uint64_t rtR0MemObjSolarisVirtToPhys(struct hat* hatSpace, caddr_t virtAddr)
{
    /* We could use paddr_t (more solaris-like) rather than uint64_t but paddr_t isn't defined for 64-bit */
    pfn_t pfn = hat_getpfnum(hatSpace, virtAddr);
    if (pfn == PFN_INVALID)
    {
        AssertMsgFailed(("rtR0MemObjSolarisVirtToPhys: hat_getpfnum for %p failed.\n", virtAddr));
        return PFN_INVALID;
    }

    /* Both works, but second will work for non-page aligned virtAddr */
#if 0
    uint64_t physAddr = PAGE_SIZE * pfn;
#else
    uint64_t physAddr = ((uint64_t)pfn << MMU_PAGESHIFT) | ((uintptr_t)virtAddr & MMU_PAGEOFFSET);
#endif
    return physAddr;
}


int rtR0MemObjNativeFree(RTR0MEMOBJ pMem)
{
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)pMem;

    switch (pMemSolaris->Core.enmType)
    {
        case RTR0MEMOBJTYPE_CONT:
            i_ddi_mem_free(pMemSolaris->Core.pv, NULL);
            break;

        case RTR0MEMOBJTYPE_PAGE:
            ddi_umem_free(pMemSolaris->Cookie);
            break;

        case RTR0MEMOBJTYPE_LOCK:
        {
            struct as *addrSpace;
            if (pMemSolaris->Core.u.Lock.R0Process == NIL_RTR0PROCESS)
                addrSpace = &kas;
            else
                addrSpace = ((proc_t *)pMemSolaris->Core.u.Lock.R0Process)->p_as;

            as_pageunlock(addrSpace, pMemSolaris->ppShadowPages, pMemSolaris->Core.pv, pMemSolaris->Core.cb, S_WRITE);
            break;
        }

        case RTR0MEMOBJTYPE_MAPPING:
        {
            struct hat *hatSpace;
            struct as *addrSpace;
            if (pMemSolaris->Core.u.Mapping.R0Process == NIL_RTR0PROCESS)
            {
                /* Kernel process*/
                hatSpace = kas.a_hat;
                addrSpace = &kas;
            }
            else
            {
                /* User process */
                proc_t *userProc = (proc_t *)pMemSolaris->Core.u.Mapping.R0Process;
                hatSpace = userProc->p_as->a_hat;
                addrSpace = userProc->p_as;
            }

            rw_enter(&addrSpace->a_lock, RW_READER);
            hat_unload(hatSpace, pMemSolaris->Core.pv, pMemSolaris->Core.cb, HAT_UNLOAD_UNLOCK);
            rw_exit(&addrSpace->a_lock);
            as_unmap(addrSpace, pMemSolaris->Core.pv, pMemSolaris->Core.cb);
            break;
        }

        /* unused */
        case RTR0MEMOBJTYPE_LOW:
        case RTR0MEMOBJTYPE_PHYS:
        case RTR0MEMOBJTYPE_RES_VIRT:
        default:
            AssertMsgFailed(("enmType=%d\n", pMemSolaris->Core.enmType));
            return VERR_INTERNAL_ERROR;
    }

    return VINF_SUCCESS;
}


int rtR0MemObjNativeAllocPage(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    /* Create the object */
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_PAGE, NULL, cb);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    void *virtAddr = ddi_umem_alloc(cb, DDI_UMEM_SLEEP, &pMemSolaris->Cookie);
    if (!virtAddr)
    {
        rtR0MemObjDelete(&pMemSolaris->Core);
        return VERR_NO_PAGE_MEMORY;
    }

    pMemSolaris->Core.pv = virtAddr;
    pMemSolaris->ppShadowPages = NULL;
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
}


int rtR0MemObjNativeAllocLow(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    /* Try page alloc first */
    int rc = rtR0MemObjNativeAllocPage(ppMem, cb, fExecutable);
    if (RT_SUCCESS(rc))
    {
        size_t iPage = cb >> PAGE_SHIFT;
        while (iPage-- > 0)
            if (rtR0MemObjNativeGetPagePhysAddr(*ppMem, iPage) > (_4G - PAGE_SIZE))
            {
                /* Failed! Fall back to physical contiguous alloc */
                RTR0MemObjFree(*ppMem, false);
                rc = rtR0MemObjNativeAllocCont(ppMem, cb, fExecutable);
                break;
            }
    }
    return rc;
}


int rtR0MemObjNativeAllocCont(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    NOREF(fExecutable);

    /* Create the object */
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_CONT, NULL, cb);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    /* Allocate physically contiguous page-aligned memory. */
    caddr_t virtAddr;
    int rc = i_ddi_mem_alloc(NULL, &g_SolarisX86PhysMemLimits, cb, 1, 0, NULL, &virtAddr, NULL, NULL);
    if (rc != DDI_SUCCESS)
    {
        rtR0MemObjDelete(&pMemSolaris->Core);
        return VERR_NO_CONT_MEMORY;
    }

    pMemSolaris->Core.pv = virtAddr;
    pMemSolaris->Core.u.Cont.Phys = rtR0MemObjSolarisVirtToPhys(kas.a_hat, virtAddr);
    pMemSolaris->ppShadowPages = NULL;
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
}


int rtR0MemObjNativeAllocPhysNC(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, RTHCPHYS PhysHighest)
{
    /** @todo rtR0MemObjNativeAllocPhysNC / solaris */
    return VERR_NOT_SUPPORTED; /* see the RTR0MemObjAllocPhysNC specs */
}


int rtR0MemObjNativeAllocPhys(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, RTHCPHYS PhysHighest)
{
    AssertMsgReturn(PhysHighest >= 16 *_1M, ("PhysHigest=%VHp\n", PhysHighest), VERR_NOT_IMPLEMENTED);

    return rtR0MemObjNativeAllocCont(ppMem, cb, false);
}


int rtR0MemObjNativeEnterPhys(PPRTR0MEMOBJINTERNAL ppMem, RTHCPHYS Phys, size_t cb)
{
    /* Create the object */
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_PHYS, NULL, cb);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    /* There is no allocation here, it needs to be mapped somewhere first */
    pMemSolaris->Core.u.Phys.fAllocated = false;
    pMemSolaris->Core.u.Phys.PhysBase = Phys;
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
}


int rtR0MemObjNativeLockUser(PPRTR0MEMOBJINTERNAL ppMem, RTR3PTR R3Ptr, size_t cb, RTR0PROCESS R0Process)
{
    /* Create the locking object */
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_LOCK, (void *)R3Ptr, cb);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    proc_t *userproc = curproc;
    if (R0Process != NIL_RTR0PROCESS)
        userproc = (proc_t *)R0Process;

    struct as *useras = userproc->p_as;
    page_t **ppl;

    /* Lock down user pages */
    int rc = as_pagelock(useras, &ppl, (caddr_t)R3Ptr, cb, S_WRITE);
    if (rc != 0)
    {
        cmn_err(CE_NOTE,"rtR0MemObjNativeLockUser: as_pagelock failed rc=%d\n", rc);
        return VERR_LOCK_FAILED;
    }

    if (!ppl)
    {
        as_pageunlock(useras, ppl, (caddr_t)R3Ptr, cb, S_WRITE);
        cmn_err(CE_NOTE, "rtR0MemObjNativeLockUser: as_pagelock failed to get shadow pages\n");
        return VERR_LOCK_FAILED;
    }

    pMemSolaris->Core.u.Lock.R0Process = (RTR0PROCESS)userproc;
    pMemSolaris->ppShadowPages = ppl;
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
}


int rtR0MemObjNativeLockKernel(PPRTR0MEMOBJINTERNAL ppMem, void *pv, size_t cb)
{
    /* Create the locking object */
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_LOCK, pv, cb);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    caddr_t virtAddr = (caddr_t)((uintptr_t)pv & (uintptr_t)PAGEMASK);
    page_t **ppl;

    /* Lock down kernel pages */
    int rc = as_pagelock(&kas, &ppl, virtAddr, cb, S_WRITE);
    if (rc != 0)
    {
        cmn_err(CE_NOTE,"rtR0MemObjNativeLockKernel: as_pagelock failed rc=%d\n", rc);
        return VERR_LOCK_FAILED;
    }

    if (!ppl)
    {
        as_pageunlock(&kas, ppl, virtAddr, cb, S_WRITE);
        cmn_err(CE_NOTE, "rtR0MemObjNativeLockKernel: failed to get shadow pages\n");
        return VERR_LOCK_FAILED;
    }

    pMemSolaris->Core.u.Lock.R0Process = NIL_RTR0PROCESS;   /* means kernel, see rtR0MemObjNativeFree() */
    pMemSolaris->ppShadowPages = ppl;
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
}


int rtR0MemObjNativeReserveKernel(PPRTR0MEMOBJINTERNAL ppMem, void *pvFixed, size_t cb, size_t uAlignment)
{
    return VERR_NOT_IMPLEMENTED;
}


int rtR0MemObjNativeReserveUser(PPRTR0MEMOBJINTERNAL ppMem, RTR3PTR R3PtrFixed, size_t cb, size_t uAlignment, RTR0PROCESS R0Process)
{
    return VERR_NOT_IMPLEMENTED;
}

int rtR0MemObjNativeMapKernel(PPRTR0MEMOBJINTERNAL ppMem, RTR0MEMOBJ pMemToMap, void *pvFixed, size_t uAlignment, unsigned fProt)
{
    PRTR0MEMOBJSOLARIS pMemToMapSolaris = (PRTR0MEMOBJSOLARIS)pMemToMap;
    size_t size = pMemToMapSolaris->Core.cb;
    void *pv = pMemToMapSolaris->Core.pv;
    pgcnt_t cPages = btop(size);
    pgcnt_t iPage;
    caddr_t addr;
    int rc;

    /* Create the mapping object */
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_MAPPING, pv, size);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    as_rangelock(&kas);
    if (pvFixed != (void *)-1)
    {
        /* Use user specified address */
        addr = (caddr_t)pvFixed;

        /* Blow away any previous mapping */
        as_unmap(&kas, addr, size);
    }
    else
    {
        /* Let the system choose an address */
        map_addr(&addr, size, 0, 1, MAP_SHARED | MAP_ANONYMOUS);
        if (addr == NULL)
        {
            as_rangeunlock(&kas);
            cmn_err(CE_NOTE, "rtR0MemObjNativeMapKernel: map_addr failed\n");
            return VERR_NO_MEMORY;
        }

        /* Check address against alignment, fail if it doesn't match */
        if ((uintptr_t)addr & (uAlignment - 1))
        {
            as_rangeunlock(&kas);
            cmn_err(CE_NOTE, "rtR0MemObjNativeMapKernel: map_addr alignment(%ld) failed.\n", uAlignment);
            return VERR_MAP_FAILED;
        }
    }

    /* Our protection masks are identical to <sys/mman.h> but we
     * need to add PROT_USER for the pages to be accessible by user
     */
    struct segvn_crargs crArgs = SEGVN_ZFOD_ARGS(fProt | PROT_USER, PROT_ALL);
    rc = as_map(&kas, addr, size, segvn_create, &crArgs);
    as_rangeunlock(&kas);
    if (rc != 0)
    {
        cmn_err(CE_NOTE, "rtR0MemObjNativeMapKernel: as_map failure.\n");
        return VERR_NO_MEMORY;
    }

    /* Map each page into kernel space */
    rw_enter(&kas.a_lock, RW_READER);
    caddr_t kernAddr = pv;
    caddr_t pageAddr = addr;
    for (iPage = 0; iPage < cPages; iPage++)
    {
        page_t *pp = page_numtopp_nolock(hat_getpfnum(kas.a_hat, kernAddr));
        hat_memload(kas.a_hat, pageAddr, pp, (fProt | PROT_USER), HAT_LOAD_LOCK);
        pageAddr += ptob(1);
        kernAddr += ptob(1);
    }
    rw_exit(&kas.a_lock);

    pMemSolaris->Core.u.Mapping.R0Process = NIL_RTR0PROCESS; /* means kernel */
    pMemSolaris->Core.pv = addr;
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
}


int rtR0MemObjNativeMapUser(PPRTR0MEMOBJINTERNAL ppMem, PRTR0MEMOBJINTERNAL pMemToMap, RTR3PTR R3PtrFixed, size_t uAlignment, unsigned fProt, RTR0PROCESS R0Process)
{
    PRTR0MEMOBJSOLARIS pMemToMapSolaris = (PRTR0MEMOBJSOLARIS)pMemToMap;
    size_t size = pMemToMapSolaris->Core.cb;
    proc_t *userproc = (proc_t *)R0Process;
    struct as *useras = userproc->p_as;
    void *pv = pMemToMapSolaris->Core.pv;
    pgcnt_t cPages = btop(size);
    pgcnt_t iPage;
    caddr_t addr;
    int rc;

    /* Create the mapping object */
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_MAPPING, pv, size);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    as_rangelock(useras);
    if (R3PtrFixed != (RTR3PTR)-1)
    {
        /* Use user specified address */
        addr = (caddr_t)R3PtrFixed;

        /* Verify user address (a bit paranoid) */
        rc = valid_usr_range(addr, size, fProt, useras, (caddr_t)USERLIMIT32);
        if (rc != RANGE_OKAY)
        {
            as_rangeunlock(useras);
            cmn_err(CE_NOTE, "rtR0MemObjNativeMapUser: valid_usr_range failed, returned %d\n", rc);
            return VERR_INVALID_POINTER;
        }

        /* Blow away any previous mapping */
        as_unmap(useras, addr, size);
    }
    else
    {
        /* Let the system choose an address */
        map_addr(&addr, size, 0, 1, MAP_SHARED | MAP_ANONYMOUS);
        if (addr == NULL)
        {
            as_rangeunlock(useras);
            cmn_err(CE_NOTE, "rtR0MemObjNativeMapUser: map_addr failed\n");
            return VERR_MAP_FAILED;
        }

        /* Check address against alignment, fail if it doesn't match */
        if ((uintptr_t)addr & (uAlignment - 1))
        {
            as_rangeunlock(useras);
            cmn_err(CE_NOTE, "rtR0MemObjNativeMapUser: map_addr alignment(%ld) failed.\n", uAlignment);
            return VERR_MAP_FAILED;
        }
    }

    /* Our protection masks are identical to <sys/mman.h> but we
     * need to add PROT_USER for the pages to be accessible by user
     */
    struct segvn_crargs crArgs = SEGVN_ZFOD_ARGS(fProt | PROT_USER, PROT_ALL);
    rc = as_map(useras, addr, size, segvn_create, &crArgs);
    as_rangeunlock(useras);
    if (rc != 0)
    {
        cmn_err(CE_NOTE, "rtR0MemObjNativeMapUser: as_map failure.\n");
        return VERR_MAP_FAILED;
    }

#if 0
    /* Lock down the pages and get the shadow page list
     * In this case we must as_pageunlock if(ppShadowPages) exists while freeing CONT, PAGE
     */
    rc = as_pagelock(&kas, &pMemToMapSolaris->ppShadowPages, pv, size, S_WRITE);
    if (rc != 0 || pMemToMapSolaris->ppShadowPages == NULL)
    {
        cmn_err(CE_NOTE, "rtR0MemObjNativeMapUser: as_pagelock failed\n");
        as_unmap(useras, addr, size);
        return VERR_NO_MEMORY;
    }

    /* Map each page into user space */
    rw_enter(&useras->a_lock, RW_READER);
    caddr_t pageAddr = addr;
    for (iPage = 0; iPage < cPages; iPage++)
    {
        hat_memload(useras->a_hat, pageAddr, pMemToMapSolaris->ppShadowPages[iPage], fProt | PROT_USER,
                HAT_LOAD_NOCONSIST | HAT_STRICTORDER | HAT_LOAD_LOCK);
        pageAddr += ptob(1);
    }
    rw_exit(&useras->a_lock, RW_READER);
#else
    /* Map each page into user space */
    rw_enter(&useras->a_lock, RW_READER);
    caddr_t kernAddr = pv;
    caddr_t pageAddr = addr;
    for (iPage = 0; iPage < cPages; iPage++)
    {
        page_t *pp = page_numtopp_nolock(hat_getpfnum(kas.a_hat, kernAddr));
        hat_memload(useras->a_hat, pageAddr, pp, (fProt | PROT_USER), HAT_LOAD_LOCK);
        pageAddr += ptob(1);
        kernAddr += ptob(1);
    }
    rw_exit(&useras->a_lock);
#endif

    pMemSolaris->Core.u.Mapping.R0Process = (RTR0PROCESS)userproc;
    pMemSolaris->Core.pv = addr;
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
}


RTHCPHYS rtR0MemObjNativeGetPagePhysAddr(PRTR0MEMOBJINTERNAL pMem, size_t iPage)
{
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)pMem;

    switch (pMemSolaris->Core.enmType)
    {
        case RTR0MEMOBJTYPE_PAGE:
        case RTR0MEMOBJTYPE_LOW:
        case RTR0MEMOBJTYPE_MAPPING:
        {
            uint8_t *pb = (uint8_t *)pMemSolaris->Core.pv + ((size_t)iPage << PAGE_SHIFT);
            return rtR0MemObjSolarisVirtToPhys(kas.a_hat, pb);
        }

        case RTR0MEMOBJTYPE_LOCK:
        {
            struct hat *hatSpace;
            if (pMemSolaris->Core.u.Lock.R0Process != NIL_RTR0PROCESS)
            {
                /* User */
                proc_t *userProc = (proc_t *)pMemSolaris->Core.u.Lock.R0Process;
                hatSpace = userProc->p_as->a_hat;
            }
            else    /* Kernel */
                hatSpace = kas.a_hat;

            uint8_t *pb = (uint8_t *)pMemSolaris->Core.pv + ((size_t)iPage << PAGE_SHIFT);
            return rtR0MemObjSolarisVirtToPhys(hatSpace, pb);
        }

        case RTR0MEMOBJTYPE_CONT:
            return pMemSolaris->Core.u.Cont.Phys + (iPage << PAGE_SHIFT);

        case RTR0MEMOBJTYPE_PHYS:
            return pMemSolaris->Core.u.Phys.PhysBase + (iPage << PAGE_SHIFT);

        case RTR0MEMOBJTYPE_PHYS_NC:
            AssertFailed(/* not implemented */);
        case RTR0MEMOBJTYPE_RES_VIRT:
        default:
            return NIL_RTHCPHYS;
    }
}

