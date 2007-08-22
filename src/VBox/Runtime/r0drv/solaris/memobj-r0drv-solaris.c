/* $Id$ */
/** @file
 * innotek Portable Runtime - Ring-0 Memory Objects, Solaris.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
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


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/


int rtR0MemObjNativeFree(RTR0MEMOBJ pMem)
{
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)pMem;

    switch (pMemSolaris->Core.enmType)
    {
        case RTR0MEMOBJTYPE_CONT:
            ddi_mem_free(pMemSolaris->Core.pv);
            break;

        case RTR0MEMOBJTYPE_PAGE:
#if 0
            ddi_umem_free(pMemSolaris->Cookie);
#endif
            ddi_mem_free(pMemSolaris->Core.pv);
            break;

        case RTR0MEMOBJTYPE_LOCK:
        {
            struct as* addrSpace;
            if (pMemSolaris->Core.u.Lock.R0Process == NIL_RTR0PROCESS)
                addrSpace = &kas;
            else
                addrSpace = ((proc_t *)pMemSolaris->Core.u.Lock.R0Process)->p_as;

            as_pageunlock(addrSpace, pMemSolaris->ppShadowPages, pMemSolaris->Core.pv, pMemSolaris->Core.cb, S_WRITE);
            break;
        }
        
        case RTR0MEMOBJTYPE_MAPPING:
        {
            if (pMemSolaris->Core.u.Mapping.R0Process == NIL_RTR0PROCESS)
            {
                /* Kernel process*/
                hat_unload(kas.a_hat, (caddr_t)pMemSolaris->Core.pv, pMemSolaris->Core.cb, HAT_UNLOAD_UNLOCK);
                vmem_xfree(heap32_arena, (caddr_t)pMemSolaris->Core.pv, pMemSolaris->Core.cb);
            }
            else
            {
                /* User process */
                proc_t *p = (proc_t *)pMemSolaris->Core.u.Mapping.R0Process;
                struct as *useras = p->p_as;
                hat_unload(useras->a_hat, (caddr_t)pMemSolaris->Core.pv, pMemSolaris->Core.cb, HAT_UNLOAD_UNLOCK);
            }
            
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
#if 1
    /* Allocate physically contiguous page-aligned memory. */
    caddr_t virtAddr;
    int rc = i_ddi_mem_alloc(NULL, &g_SolarisX86PhysMemLimits, cb, 1, 0, NULL, &virtAddr, NULL, NULL);
    if (rc != DDI_SUCCESS)
    {
        rtR0MemObjDelete(&pMemSolaris->Core);
        return VERR_NO_MEMORY;
    }
    
    pMemSolaris->Core.pv = virtAddr;
    pMemSolaris->Core.u.Cont.Phys = PAGE_SIZE * hat_getpfnum(kas.a_hat, virtAddr);
    *ppMem = &pMemSolaris->Core;    
    cmn_err(CE_NOTE, "xAllocPage success physAddr=%p virt=%p\n", PAGE_SIZE * hat_getpfnum(kas.a_hat, virtAddr), virtAddr);
#endif
#if 0
    /* Allocate page-aligned kernel memory */
    void *pv = ddi_umem_alloc(cb, DDI_UMEM_SLEEP, &pMemSolaris->Cookie);
    if (pv == NULL)
    {
        rtR0MemObjDelete(&pMemSolaris->Core);
        return VERR_NO_MEMORY;
    }
    
    pMemSolaris->Core.pv = pv;
    *ppMem = &pMemSolaris->Core;
    cmn_err(CE_NOTE, "ddi_umem_alloc, success\n");
#endif        
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
                cmn_err(CE_NOTE, "4G boundary exceeded\n");
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
        return VERR_NO_MEMORY;
    }
    
    pMemSolaris->Core.pv = virtAddr;
    pMemSolaris->Core.u.Cont.Phys = PAGE_SIZE * hat_getpfnum(kas.a_hat, virtAddr);
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
}


int rtR0MemObjNativeAllocPhysNC(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, RTHCPHYS PhysHighest)
{
    /** @todo rtR0MemObjNativeAllocPhys / solaris */
    return rtR0MemObjNativeAllocPhys(ppMem, cb, PhysHighest);
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
    
    /* @todo validate Phys as a proper physical address */

    /* There is no allocation here, it needs to be mapped somewhere first */
    pMemSolaris->Core.u.Phys.fAllocated = false;
    pMemSolaris->Core.u.Phys.PhysBase = Phys;
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
}


int rtR0MemObjNativeLockUser(PPRTR0MEMOBJINTERNAL ppMem, RTR3PTR R3Ptr, size_t cb, RTR0PROCESS R0Process)
{
    /* Create the object */
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_LOCK, (void*)R3Ptr, cb);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    proc_t *userProcess = curproc;
    if (R0Process != NIL_RTR0PROCESS)
        userProcess = (proc_t *)R0Process;
    
    struct as* userAddrSpace = userProcess->p_as;
    caddr_t userAddr = (caddr_t)((uintptr_t)R3Ptr & (uintptr_t)PAGEMASK);
    page_t **ppl;

    int rc = as_pagelock(userAddrSpace, &ppl, userAddr, cb, S_WRITE);
    if (rc != 0)
        return VERR_NO_MEMORY;

    pMemSolaris->Core.u.Lock.R0Process = (RTR0PROCESS)userProcess;
    pMemSolaris->ppShadowPages = ppl;
    *ppMem = &pMemSolaris->Core;
    
    return VINF_SUCCESS;
    
#if 0    
    /* Lock down the physical pages of current process' virtual address space */
    int rc = ddi_umem_lock(pv, cb, DDI_UMEMLOCK_WRITE, &pMemSolaris->Cookie);
    if (rc != 0)
    {
        rtR0MemObjDelete(&pMemSolaris->Core);
        return VERR_NO_MEMORY;  /** @todo fix mach -> vbox error conversion for Solaris. */
    }

    pMemSolaris->Core.u.Lock.R0Process = R0Process;
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
#endif    
}


int rtR0MemObjNativeLockKernel(PPRTR0MEMOBJINTERNAL ppMem, void *pv, size_t cb)
{
    /* Create the object */
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_LOCK, pv, cb);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    caddr_t userAddr = (caddr_t)((uintptr_t)pv & (uintptr_t)PAGEMASK);
    page_t **ppl;
    
    int rc = as_pagelock(&kas, &ppl, userAddr, cb, S_WRITE);
    if (rc != 0)
        return VERR_NO_MEMORY;

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
    size_t size = P2ROUNDUP(pMemToMapSolaris->Core.cb, PAGE_SIZE);
    void* pv = pMemToMapSolaris->Core.pv;
    
    void* kernVirtAddr = vmem_xalloc(heap32_arena, size, PAGE_SIZE, 0, PAGE_SIZE, NULL, 0, VM_SLEEP);
    if (kernVirtAddr == NULL)
        return VERR_NO_MEMORY;
    
    hat_devload(kas.a_hat, (caddr_t)kernVirtAddr, size, hat_getpfnum(kas.a_hat, pv), PROT_READ | PROT_WRITE | PROT_EXEC,
                HAT_STRICTORDER | HAT_LOAD_NOCONSIST | HAT_LOAD_LOCK);
    
    /* Create the mapping object */
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_MAPPING,
                                                                    kernVirtAddr, pMemToMapSolaris->Core.cb);
    if (pMemSolaris == NULL)
    {
        hat_unload(kas.a_hat, (caddr_t)kernVirtAddr, size, HAT_UNLOAD_UNLOCK);
        vmem_xfree(heap32_arena, kernVirtAddr, size);
        return VERR_NO_MEMORY;
    }

    pMemSolaris->Core.u.Mapping.R0Process = NIL_RTR0PROCESS;    /* NIL_RTR0PROCESS means kernel process */
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
}


int rtR0MemObjNativeMapUser(PPRTR0MEMOBJINTERNAL ppMem, PRTR0MEMOBJINTERNAL pMemToMap, RTR3PTR R3PtrFixed, size_t uAlignment, unsigned fProt, RTR0PROCESS R0Process)
{
    PRTR0MEMOBJSOLARIS pMemToMapSolaris = (PRTR0MEMOBJSOLARIS)pMemToMap;    
    size_t size = P2ROUNDUP(pMemToMapSolaris->Core.cb, PAGE_SIZE);
    proc_t *userproc = (proc_t *)R0Process;
    struct as *useras = userproc->p_as;
    void *pv = pMemToMapSolaris->Core.pv;
    pfn_t pfnum = hat_getpfnum(kas.a_hat, pv);
    int rc;
    
    void* kernVirtAddr = vmem_xalloc(heap32_arena, size, PAGE_SIZE, 0, PAGE_SIZE, NULL, 0, VM_SLEEP);
    if (kernVirtAddr == NULL)
        return VERR_NO_MEMORY;
    
    cmn_err(CE_NOTE, "vmem_xalloc successful.\n");
    
    /* Wrong ones to use: as_map() */
    hat_devload(kas.a_hat, (caddr_t)kernVirtAddr, size, pfnum, PROT_READ | PROT_WRITE | PROT_EXEC,
                    HAT_STRICTORDER | HAT_LOAD_NOCONSIST | HAT_LOAD_LOCK);
    
    cmn_err(CE_NOTE, "hat_devload successful.\n");
    
    /* Create the mapping object */
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_MAPPING,
                                                                    pv, pMemToMapSolaris->Core.cb);
    if (pMemSolaris == NULL)
    {
        /* @todo cleanup */
        return VERR_NO_MEMORY;
    }

    pMemSolaris->Core.u.Mapping.R0Process = R0Process;
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
}


RTHCPHYS rtR0MemObjNativeGetPagePhysAddr(PRTR0MEMOBJINTERNAL pMem, unsigned iPage)
{
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)pMem;

    switch (pMemSolaris->Core.enmType)
    {
        case RTR0MEMOBJTYPE_LOCK:
        {
            /* @todo figure this one out */
            return NIL_RTHCPHYS;
        }
        
        case RTR0MEMOBJTYPE_PAGE:
        case RTR0MEMOBJTYPE_LOW:
        {
            uint8_t *pb = (uint8_t *)pMemSolaris->Core.pv + ((size_t)iPage << PAGE_SHIFT);
            return PAGE_SIZE * hat_getpfnum(kas.a_hat, pb);
        }

        case RTR0MEMOBJTYPE_CONT:
            return pMemSolaris->Core.u.Cont.Phys + (iPage << PAGE_SHIFT);

        case RTR0MEMOBJTYPE_PHYS:
            return pMemSolaris->Core.u.Phys.PhysBase + (iPage << PAGE_SHIFT);

        case RTR0MEMOBJTYPE_RES_VIRT:
        case RTR0MEMOBJTYPE_MAPPING:
        default:
            return NIL_RTHCPHYS;
    }
}
