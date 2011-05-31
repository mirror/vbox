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

#include <iprt/asm.h>
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
    /** Set if large pages are involved in an RTR0MEMOBJTYPE_PHYS
     *  allocation. */
    bool                fLargePage;
} RTR0MEMOBJSOLARIS, *PRTR0MEMOBJSOLARIS;



DECLHIDDEN(int) rtR0MemObjNativeFree(RTR0MEMOBJ pMem)
{
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)pMem;

    switch (pMemSolaris->Core.enmType)
    {
        case RTR0MEMOBJTYPE_LOW:
            vbi_lowmem_free(pMemSolaris->Core.pv, pMemSolaris->Core.cb);
            break;

        case RTR0MEMOBJTYPE_PHYS:
            if (!pMemSolaris->Core.u.Phys.fAllocated)
            {   /* nothing to do here */;   }
            else if (pMemSolaris->fLargePage)
                vbi_large_page_free(pMemSolaris->pvHandle, pMemSolaris->Core.cb);
            else
                vbi_phys_free(pMemSolaris->Core.pv, pMemSolaris->Core.cb);
            break;

        case RTR0MEMOBJTYPE_PHYS_NC:
            vbi_pages_free(pMemSolaris->pvHandle, pMemSolaris->Core.cb);
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
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_PAGE, NULL, cb);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    void *virtAddr = ddi_umem_alloc(cb, DDI_UMEM_SLEEP, &pMemSolaris->Cookie);
    if (!virtAddr)
    {
        rtR0MemObjDelete(&pMemSolaris->Core);
        return VERR_NO_PAGE_MEMORY;
    }

    pMemSolaris->Core.pv  = virtAddr;
    pMemSolaris->pvHandle = NULL;
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
}


DECLHIDDEN(int) rtR0MemObjNativeAllocLow(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
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


DECLHIDDEN(int) rtR0MemObjNativeAllocCont(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    NOREF(fExecutable);
    return rtR0MemObjNativeAllocPhys(ppMem, cb, _4G - 1, PAGE_SIZE /* alignment */);
}


DECLHIDDEN(int) rtR0MemObjNativeAllocPhysNC(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, RTHCPHYS PhysHighest)
{
#if HC_ARCH_BITS == 64
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_PHYS_NC, NULL, cb);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    uint64_t PhysAddr = PhysHighest;
    void *pvPages = vbi_pages_alloc(&PhysAddr, cb);
    if (!pvPages)
    {
        LogRel(("rtR0MemObjNativeAllocPhysNC: vbi_pages_alloc failed.\n"));
        rtR0MemObjDelete(&pMemSolaris->Core);
        return VERR_NO_MEMORY;
    }
    pMemSolaris->Core.pv   = NULL;
    pMemSolaris->pvHandle  = pvPages;

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

    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_PHYS, NULL, cb);
    if (!pMemSolaris)
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
        void *pvPages = vbi_large_page_alloc(&PhysAddr, cb);
        if (pvPages)
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
        caddr_t pvMem = vbi_phys_alloc(&PhysAddr, cb, uAlignment, 1 /* contiguous */);
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
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_PHYS, NULL, cb);
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
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_LOCK, (void *)R3Ptr, cb);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    /* Lock down user pages. */
    int fPageAccess = S_READ;
    if (fAccess & RTMEM_PROT_WRITE)
        fPageAccess = S_WRITE;
    if (fAccess & RTMEM_PROT_EXEC)
        fPageAccess = S_EXEC;
    void *pvPageList = NULL;
    int rc = vbi_lock_va((caddr_t)R3Ptr, cb, fPageAccess, &pvPageList);
    if (rc != 0)
    {
        LogRel(("rtR0MemObjNativeLockUser: vbi_lock_va failed rc=%d\n", rc));
        rtR0MemObjDelete(&pMemSolaris->Core);
        return VERR_LOCK_FAILED;
    }

    /* Fill in the object attributes and return successfully. */
    pMemSolaris->Core.u.Lock.R0Process  = R0Process;
    pMemSolaris->pvHandle               = pvPageList;
    pMemSolaris->fAccess                = fPageAccess;
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
}


DECLHIDDEN(int) rtR0MemObjNativeLockKernel(PPRTR0MEMOBJINTERNAL ppMem, void *pv, size_t cb, uint32_t fAccess)
{
    NOREF(fAccess);

    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_LOCK, pv, cb);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    /* Lock down kernel pages. */
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

    /* Fill in the object attributes and return successfully. */
    pMemSolaris->Core.u.Lock.R0Process = NIL_RTR0PROCESS;
    pMemSolaris->pvHandle = pvPageList;
    pMemSolaris->fAccess = fPageAccess;
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
}


DECLHIDDEN(int) rtR0MemObjNativeReserveKernel(PPRTR0MEMOBJINTERNAL ppMem, void *pvFixed, size_t cb, size_t uAlignment)
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
    PRTR0MEMOBJSOLARIS  pMemToMapSolaris = (PRTR0MEMOBJSOLARIS)pMemToMap;
    void               *pv               = pMemToMapSolaris->Core.pv;
    size_t              cb               = pMemToMapSolaris->Core.cb;
    pgcnt_t             cPages           = cb >> PAGE_SHIFT;

    /*
     * Create the mapping object
     */
    PRTR0MEMOBJSOLARIS pMemSolaris;
    pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_MAPPING, pv, cb);
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
            rc = vbi_pages_premap(pMemToMapSolaris->pvHandle, cb, paPhysAddrs);
        else if (   pMemToMapSolaris->Core.enmType == RTR0MEMOBJTYPE_PHYS
                 && pMemToMapSolaris->fLargePage)
        {
            RTHCPHYS Phys = pMemToMapSolaris->Core.u.Phys.PhysBase;
            for (pgcnt_t iPage = 0; iPage < cPages; iPage++, Phys += PAGE_SIZE)
                paPhysAddrs[iPage] = Phys;
            rc = vbi_large_page_premap(pMemToMapSolaris->pvHandle, cb);
        }
        else
        {
            /* Have kernel mapping, just translate virtual to physical. */
            AssertPtr(pv);
            rc = 0;
            for (pgcnt_t iPage = 0; iPage < cPages; iPage++)
            {
                paPhysAddrs[iPage] = vbi_va_to_pa(pv);
                if (RT_UNLIKELY(paPhysAddrs[iPage] == -(uint64_t)1))
                {
                    LogRel(("rtR0MemObjNativeMapUser: no page to map.\n"));
                    rc = -1;
                    break;
                }
                pv = (void *)((uintptr_t)pv + PAGE_SIZE);
            }
        }
        if (!rc)
        {
            /*
             * Perform the actual mapping.
             */
            caddr_t UserAddr = NULL;
            rc = vbi_user_map(&UserAddr, fProt, paPhysAddrs, cb);
            if (!rc)
            {
                pMemSolaris->Core.u.Mapping.R0Process = R0Process;
                pMemSolaris->Core.pv                  = UserAddr;

                *ppMem = &pMemSolaris->Core;
                kmem_free(paPhysAddrs, sizeof(uint64_t) * cPages);
                return VINF_SUCCESS;
            }

            LogRel(("rtR0MemObjNativeMapUser: vbi_user_map failed.\n"));
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
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)pMem;

    switch (pMemSolaris->Core.enmType)
    {
        case RTR0MEMOBJTYPE_PHYS_NC:
            if (pMemSolaris->Core.u.Phys.fAllocated)
            {
                uint8_t *pb = (uint8_t *)pMemSolaris->Core.pv + ((size_t)iPage << PAGE_SHIFT);
                return vbi_va_to_pa(pb);
            }
            return vbi_page_to_pa(pMemSolaris->pvHandle, iPage);

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
            AssertFailed(); /* handled by the caller */
        case RTR0MEMOBJTYPE_RES_VIRT:
        default:
            return NIL_RTHCPHYS;
    }
}

