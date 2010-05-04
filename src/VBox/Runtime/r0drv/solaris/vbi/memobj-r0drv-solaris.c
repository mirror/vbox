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
#include "../the-solaris-kernel.h"
#include "internal/iprt.h"
#include <iprt/memobj.h>

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/log.h>
#include <iprt/mem.h>
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
    void               *pvHandle;
    /** Access during locking. */
    int                 fAccess;
} RTR0MEMOBJSOLARIS, *PRTR0MEMOBJSOLARIS;



int rtR0MemObjNativeFree(RTR0MEMOBJ pMem)
{
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)pMem;

    switch (pMemSolaris->Core.enmType)
    {
        case RTR0MEMOBJTYPE_LOW:
            vbi_lowmem_free(pMemSolaris->Core.pv, pMemSolaris->Core.cb);
            break;

        case RTR0MEMOBJTYPE_CONT:
        case RTR0MEMOBJTYPE_PHYS:
            vbi_phys_free(pMemSolaris->Core.pv, pMemSolaris->Core.cb);
            break;

        case RTR0MEMOBJTYPE_PHYS_NC:
#if 0
            vbi_phys_free(pMemSolaris->Core.pv, pMemSolaris->Core.cb);
#else
            if (pMemSolaris->Core.u.Phys.fAllocated == true)
                ddi_umem_free(pMemSolaris->Cookie);
            else
                vbi_pages_free(pMemSolaris->pvHandle, pMemSolaris->Core.cb);
#endif
            break;

        case RTR0MEMOBJTYPE_PAGE:
            ddi_umem_free(pMemSolaris->Cookie);
            break;

        case RTR0MEMOBJTYPE_LOCK:
            vbi_unlock_va(pMemSolaris->Core.pv, pMemSolaris->Core.cb, pMemSolaris->fAccess, pMemSolaris->pvHandle);
            break;

        case RTR0MEMOBJTYPE_MAPPING:
            vbi_unmap(pMemSolaris->Core.pv, pMemSolaris->Core.cb);
            break;

        case RTR0MEMOBJTYPE_RES_VIRT:
        {
            if (pMemSolaris->Core.u.ResVirt.R0Process == NIL_RTR0PROCESS)
                vmem_xfree(heap_arena, pMemSolaris->Core.pv, pMemSolaris->Core.cb);
            else
                AssertFailed();
            break;
        }

        default:
            AssertMsgFailed(("enmType=%d\n", pMemSolaris->Core.enmType));
            return VERR_INTERNAL_ERROR;
    }

    return VINF_SUCCESS;
}


int rtR0MemObjNativeAllocPage(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    /* Create the object. */
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
    pMemSolaris->pvHandle = NULL;
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
}


int rtR0MemObjNativeAllocLow(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    NOREF(fExecutable);

    /* Create the object */
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_LOW, NULL, cb);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    /* Allocate physically low page-aligned memory. */
    uint64_t physAddr = _4G - 1;
    caddr_t virtAddr  = vbi_lowmem_alloc(physAddr, cb);
    if (virtAddr == NULL)
    {
        rtR0MemObjDelete(&pMemSolaris->Core);
        return VERR_NO_LOW_MEMORY;
    }
    pMemSolaris->Core.pv = virtAddr;
    pMemSolaris->pvHandle = NULL;
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
}


int rtR0MemObjNativeAllocCont(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    NOREF(fExecutable);
    return rtR0MemObjNativeAllocPhys(ppMem, cb, _4G - 1, PAGE_SIZE /* alignment */);
}


int rtR0MemObjNativeAllocPhysNC(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, RTHCPHYS PhysHighest)
{
#if HC_ARCH_BITS == 64
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_PHYS_NC, NULL, cb);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    /* Allocate physically non-contiguous page-aligned memory. */
    uint64_t physAddr = PhysHighest;

#if 0
    /*
     * The contig_alloc() way of allocating NC pages is broken or does not match our semantics. Refer #4716 for details.
     */
    /* caddr_t virtAddr  = vbi_phys_alloc(&physAddr, cb, PAGE_SIZE, 0 /* non-contiguous */); */
    caddr_t virtAddr = ddi_umem_alloc(cb, DDI_UMEM_SLEEP, &pMemSolaris->Cookie);
    if (RT_UNLIKELY(virtAddr == NULL))
    {
        rtR0MemObjDelete(&pMemSolaris->Core);
        return VERR_NO_MEMORY;
    }
    pMemSolaris->Core.pv = virtAddr;
    pMemSolaris->Core.u.Phys.PhysBase = physAddr;
    pMemSolaris->Core.u.Phys.fAllocated = true;
    pMemSolaris->pvHandle = NULL;
#else
    void *pvPages = vbi_pages_alloc(&physAddr, cb);
    if (!pvPages)
    {
        LogRel(("rtR0MemObjNativeAllocPhysNC: vbi_pages_alloc failed.\n"));
        rtR0MemObjDelete(&pMemSolaris->Core);
        return VERR_NO_MEMORY;
    }
    pMemSolaris->Core.pv = NULL;
    pMemSolaris->Core.u.Phys.PhysBase = physAddr;
    pMemSolaris->Core.u.Phys.fAllocated = false;
    pMemSolaris->pvHandle = pvPages;
#endif

    Assert(!(physAddr & PAGE_OFFSET_MASK));
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
#else
    /** @todo rtR0MemObjNativeAllocPhysNC / solaris */
    return VERR_NOT_SUPPORTED; /* see the RTR0MemObjAllocPhysNC specs */
#endif
}


int rtR0MemObjNativeAllocPhys(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, RTHCPHYS PhysHighest, size_t uAlignment)
{
    AssertMsgReturn(PhysHighest >= 16 *_1M, ("PhysHigest=%RHp\n", PhysHighest), VERR_NOT_IMPLEMENTED);

    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_PHYS, NULL, cb);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    AssertCompile(NIL_RTHCPHYS == UINT64_MAX);

    /* Allocate physically contiguous memory aligned as specified. */
    uint64_t physAddr = PhysHighest;
    caddr_t  virtAddr = vbi_phys_alloc(&physAddr, cb, uAlignment, 1 /* contiguous */);
    if (RT_UNLIKELY(virtAddr == NULL))
    {
        rtR0MemObjDelete(&pMemSolaris->Core);
        return VERR_NO_CONT_MEMORY;
    }
    Assert(!(physAddr & PAGE_OFFSET_MASK));
    Assert(physAddr < PhysHighest);
    Assert(physAddr + cb <= PhysHighest);
#if 0
    if (uAlignment != PAGE_SIZE)
    {
        /* uAlignment is always a multiple of PAGE_SIZE */
        pgcnt_t cPages = (cb + uAlignment - 1) >> PAGE_SHIFT;
        void *pvPage = virtAddr;
        while (cPages-- > 0)
        {
            uint64_t u64Page = vbi_va_to_pa(pvPage);
            if (u64Page & (uAlignment - 1))
            {
                LogRel(("rtR0MemObjNativeAllocPhys: alignment mismatch! cb=%u uAlignment=%u physAddr=%#x\n", cb, uAlignment, u64Page));
                vbi_phys_free(virtAddr, cb);
                rtR0MemObjDelete(&pMemSolaris->Core);
                return VERR_NO_MEMORY;
            }
            pvPage = (void *)((uintptr_t)pvPage + PAGE_SIZE);
        }
    }
#endif
    pMemSolaris->Core.pv = virtAddr;
    pMemSolaris->Core.u.Cont.Phys = physAddr;
    pMemSolaris->pvHandle = NULL;
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
}


int rtR0MemObjNativeEnterPhys(PPRTR0MEMOBJINTERNAL ppMem, RTHCPHYS Phys, size_t cb, uint32_t uCachePolicy)
{
    AssertReturn(uCachePolicy == RTMEM_CACHE_POLICY_DONT_CARE, VERR_NOT_IMPLEMENTED);

    /* Create the object. */
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_PHYS, NULL, cb);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    /* There is no allocation here, it needs to be mapped somewhere first. */
    pMemSolaris->Core.u.Phys.fAllocated = false;
    pMemSolaris->Core.u.Phys.PhysBase = Phys;
    pMemSolaris->Core.u.Phys.uCachePolicy = uCachePolicy;
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
}


int rtR0MemObjNativeLockUser(PPRTR0MEMOBJINTERNAL ppMem, RTR3PTR R3Ptr, size_t cb, uint32_t fAccess, RTR0PROCESS R0Process)
{
    AssertReturn(R0Process == RTR0ProcHandleSelf(), VERR_INVALID_PARAMETER);
    NOREF(fAccess);

    /* Create the locking object */
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_LOCK, (void *)R3Ptr, cb);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    int fPageAccess = S_READ;
    if (fAccess & RTMEM_PROT_WRITE)
        fPageAccess = S_WRITE;
    if (fAccess & RTMEM_PROT_EXEC)
        fPageAccess = S_EXEC;
    void *pvPageList = NULL;

    /* Lock down user pages */
    int rc = vbi_lock_va((caddr_t)R3Ptr, cb, fPageAccess, &pvPageList);
    if (rc != 0)
    {
        LogRel(("rtR0MemObjNativeLockUser: vbi_lock_va failed rc=%d\n", rc));
        rtR0MemObjDelete(&pMemSolaris->Core);
        return VERR_LOCK_FAILED;
    }

    pMemSolaris->Core.u.Lock.R0Process = (RTR0PROCESS)vbi_proc();
    pMemSolaris->pvHandle = pvPageList;
    pMemSolaris->fAccess = fPageAccess;
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
}


int rtR0MemObjNativeLockKernel(PPRTR0MEMOBJINTERNAL ppMem, void *pv, size_t cb, uint32_t fAccess)
{
    NOREF(fAccess);

    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_LOCK, pv, cb);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    int fPageAccess = S_READ;
    if (fAccess & RTMEM_PROT_WRITE)
        fPageAccess = S_WRITE;
    if (fAccess & RTMEM_PROT_EXEC)
        fPageAccess = S_EXEC;
    void *pvPageList = NULL;
    int rc = vbi_lock_va((caddr_t)pv, cb, fPageAccess, &pvPageList);
    if (rc != 0)
    {
        LogRel(("rtR0MemObjNativeLockKernel: vbi_lock_va failed rc=%d\n", rc));
        rtR0MemObjDelete(&pMemSolaris->Core);
        return VERR_LOCK_FAILED;
    }

    pMemSolaris->Core.u.Lock.R0Process = NIL_RTR0PROCESS;
    pMemSolaris->pvHandle = pvPageList;
    pMemSolaris->fAccess = fPageAccess;
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
}


int rtR0MemObjNativeReserveKernel(PPRTR0MEMOBJINTERNAL ppMem, void *pvFixed, size_t cb, size_t uAlignment)
{
    PRTR0MEMOBJSOLARIS  pMemSolaris;

    /*
     * Use xalloc.
     */
    void *pv = vmem_xalloc(heap_arena, cb, uAlignment, 0 /*phase*/, 0 /*nocross*/,
                     NULL /*minaddr*/, NULL /*maxaddr*/, VM_SLEEP);
    if (RT_UNLIKELY(!pv))
        return VERR_NO_MEMORY;

    /* Create the object. */
    pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_RES_VIRT, pv, cb);
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


int rtR0MemObjNativeReserveUser(PPRTR0MEMOBJINTERNAL ppMem, RTR3PTR R3PtrFixed, size_t cb, size_t uAlignment, RTR0PROCESS R0Process)
{
    return VERR_NOT_IMPLEMENTED;
}

int rtR0MemObjNativeMapKernel(PPRTR0MEMOBJINTERNAL ppMem, RTR0MEMOBJ pMemToMap, void *pvFixed, size_t uAlignment,
                              unsigned fProt, size_t offSub, size_t cbSub)
{
    /** @todo rtR0MemObjNativeMapKernel / Solaris - Should be fairly simple alloc kernel memory and memload it. */
    return VERR_NOT_IMPLEMENTED;
}


int rtR0MemObjNativeMapUser(PPRTR0MEMOBJINTERNAL ppMem, PRTR0MEMOBJINTERNAL pMemToMap, RTR3PTR R3PtrFixed, size_t uAlignment, unsigned fProt, RTR0PROCESS R0Process)
{
    AssertMsgReturn(R3PtrFixed == (RTR3PTR)-1, ("%p\n", R3PtrFixed), VERR_NOT_SUPPORTED);
    AssertMsgReturn(R0Process == RTR0ProcHandleSelf(), ("%p != %p\n", R0Process, RTR0ProcHandleSelf()), VERR_NOT_SUPPORTED);
    if (uAlignment != PAGE_SIZE)
        return VERR_NOT_SUPPORTED;

    PRTR0MEMOBJSOLARIS pMemToMapSolaris = (PRTR0MEMOBJSOLARIS)pMemToMap;
    size_t cb        = pMemToMapSolaris->Core.cb;
    void *pv         = pMemToMapSolaris->Core.pv;
    pgcnt_t cPages   = (cb + PAGE_SIZE - 1) >> PAGE_SHIFT;

    /* Create the mapping object */
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_MAPPING, pv, cb);
    if (RT_UNLIKELY(!pMemSolaris))
        return VERR_NO_MEMORY;

    uint64_t *paPhysAddrs = kmem_zalloc(sizeof(uint64_t) * cPages, KM_SLEEP);
    if (RT_UNLIKELY(!paPhysAddrs))
        return VERR_NO_MEMORY;

    if (   pMemToMapSolaris->Core.enmType == RTR0MEMOBJTYPE_PHYS_NC
        && pMemSolaris->Core.u.Phys.fAllocated == false)
    {
        /*
         * The PhysNC object has no kernel mapping backing it. The call to vbi_pages_premap()
         * prepares the physical pages to be mapped into user or kernel space.
         */
        int rc = vbi_pages_premap(pMemToMapSolaris->pvHandle, cb, paPhysAddrs);
        if (rc)
        {
            LogRel(("rtR0MemObjNativeMapUser: vbi_pages_premap failed. rc=%d\n", rc));
            kmem_free(paPhysAddrs, sizeof(uint64_t) * cPages);
            rtR0MemObjDelete(&pMemSolaris->Core);
            return VERR_MAP_FAILED;
        }
    }
    else
    {
        /*
         * All other memory object types have allocated memory with kernel mappings.
         */
        for (pgcnt_t iPage = 0; iPage < cPages; iPage++)
        {
            paPhysAddrs[iPage] = vbi_va_to_pa(pv);
            if (RT_UNLIKELY(paPhysAddrs[iPage] == -(uint64_t)1))
            {
                LogRel(("rtR0MemObjNativeMapUser: no page to map.\n"));
                kmem_free(paPhysAddrs, sizeof(uint64_t) * cPages);
                rtR0MemObjDelete(&pMemSolaris->Core);
                return VERR_MAP_FAILED;
            }
            pv = (void *)((uintptr_t)pv + PAGE_SIZE);
        }
    }

    caddr_t virtAddr = NULL;
    int rc = vbi_user_map(&virtAddr, fProt, paPhysAddrs, cb);
    if (rc != 0)
    {
        LogRel(("rtR0MemObjNativeMapUser: vbi mapping failure.\n"));
        kmem_free(paPhysAddrs, sizeof(uint64_t) * cPages);
        rtR0MemObjDelete(&pMemSolaris->Core);
        return VERR_MAP_FAILED;
    }
    else
        rc = VINF_SUCCESS;

    pMemSolaris->Core.u.Mapping.R0Process = (RTR0PROCESS)vbi_proc();
    pMemSolaris->Core.pv = virtAddr;
    *ppMem = &pMemSolaris->Core;
    kmem_free(paPhysAddrs, sizeof(uint64_t) * cPages);
    return rc;
}


int rtR0MemObjNativeProtect(PRTR0MEMOBJINTERNAL pMem, size_t offSub, size_t cbSub, uint32_t fProt)
{
    NOREF(pMem);
    NOREF(offSub);
    NOREF(cbSub);
    NOREF(fProt);
    return VERR_NOT_SUPPORTED;
}


RTHCPHYS rtR0MemObjNativeGetPagePhysAddr(PRTR0MEMOBJINTERNAL pMem, size_t iPage)
{
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)pMem;

    switch (pMemSolaris->Core.enmType)
    {
        case RTR0MEMOBJTYPE_PAGE:
        case RTR0MEMOBJTYPE_LOW:
        case RTR0MEMOBJTYPE_LOCK:
        {
            uint8_t *pb = (uint8_t *)pMemSolaris->Core.pv + ((size_t)iPage << PAGE_SHIFT);
            return vbi_va_to_pa(pb);
        }

        /*
         * Although mapping can be handled by vbi_va_to_pa(offset) like the above case,
         * request it from the parent so that we have a clear distinction between CONT/PHYS_NC.
         */
        case RTR0MEMOBJTYPE_MAPPING:
            return rtR0MemObjNativeGetPagePhysAddr(pMemSolaris->Core.uRel.Child.pParent, iPage);

        case RTR0MEMOBJTYPE_CONT:
        case RTR0MEMOBJTYPE_PHYS:
            return pMemSolaris->Core.u.Cont.Phys + (iPage << PAGE_SHIFT);

        case RTR0MEMOBJTYPE_PHYS_NC:
            if (pMemSolaris->Core.u.Phys.fAllocated == true)
            {
                uint8_t *pb = (uint8_t *)pMemSolaris->Core.pv + ((size_t)iPage << PAGE_SHIFT);
                return vbi_va_to_pa(pb);
            }
            return vbi_page_to_pa(pMemSolaris->pvHandle, iPage);

        case RTR0MEMOBJTYPE_RES_VIRT:
        default:
            return NIL_RTHCPHYS;
    }
}

