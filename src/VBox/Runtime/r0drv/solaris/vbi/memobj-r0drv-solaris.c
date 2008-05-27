/* $Id$ */
/** @file
 * IPRT - Ring-0 Memory Objects, Solaris.
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
    void              *handle;
} RTR0MEMOBJSOLARIS, *PRTR0MEMOBJSOLARIS;



int rtR0MemObjNativeFree(RTR0MEMOBJ pMem)
{
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)pMem;

    switch (pMemSolaris->Core.enmType)
    {
        case RTR0MEMOBJTYPE_CONT:
            vbi_contig_free(pMemSolaris->Core.pv, pMemSolaris->Core.cb);
            break;

        case RTR0MEMOBJTYPE_PAGE:
            ddi_umem_free(pMemSolaris->Cookie);
            break;

        case RTR0MEMOBJTYPE_LOCK:
            vbi_unlock_va(pMemSolaris->Core.pv, pMemSolaris->Core.cb, pMemSolaris->handle);
            break;

        case RTR0MEMOBJTYPE_MAPPING:
            vbi_unmap(pMemSolaris->Core.pv, pMemSolaris->Core.cb);
            break;

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
    pMemSolaris->handle = NULL;
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
    uint64_t phys = (unsigned)0xffffffff;
    virtAddr = vbi_contig_alloc(&phys, cb);
    if (virtAddr == NULL)
    {
        rtR0MemObjDelete(&pMemSolaris->Core);
        return VERR_NO_CONT_MEMORY;
    }
    Assert(phys < (uint64_t)1 << 32);
    pMemSolaris->Core.pv = virtAddr;
    pMemSolaris->Core.u.Cont.Phys = phys;
    pMemSolaris->handle = NULL;
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
    AssertReturn(R0Process == RTR0ProcHandleSelf(), VERR_INVALID_PARAMETER);

    /* Create the locking object */
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_LOCK, (void *)R3Ptr, cb);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    void *ppl;

    /* Lock down user pages */
    int rc = vbi_lock_va((caddr_t)R3Ptr, cb, &ppl);
    if (rc != 0)
    {
        cmn_err(CE_NOTE,"rtR0MemObjNativeLockUser: vbi_lock_va failed rc=%d\n", rc);
        return VERR_LOCK_FAILED;
    }

    pMemSolaris->Core.u.Lock.R0Process = (RTR0PROCESS)vbi_proc();
    pMemSolaris->handle = ppl;
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
}


int rtR0MemObjNativeLockKernel(PPRTR0MEMOBJINTERNAL ppMem, void *pv, size_t cb)
{
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_LOCK, pv, cb);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    void *ppl;
    int rc = vbi_lock_va((caddr_t)pv, cb, &ppl);
    if (rc != 0)
    {
        cmn_err(CE_NOTE,"rtR0MemObjNativeLockKernel: vbi_lock_va failed rc=%d\n", rc);
        return VERR_LOCK_FAILED;
    }

    pMemSolaris->Core.u.Lock.R0Process = NIL_RTR0PROCESS;
    pMemSolaris->handle = ppl;
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
    /* @todo rtR0MemObjNativeMapKernel / Solaris - Should be fairly simple alloc kernel memory and memload it. */
    return VERR_NOT_IMPLEMENTED;
}

int rtR0MemObjNativeMapUser(PPRTR0MEMOBJINTERNAL ppMem, PRTR0MEMOBJINTERNAL pMemToMap, RTR3PTR R3PtrFixed, size_t uAlignment, unsigned fProt, RTR0PROCESS R0Process)
{
    AssertMsgReturn(R3PtrFixed == (RTR3PTR)-1, ("%p\n", R3PtrFixed), VERR_NOT_SUPPORTED);
    AssertMsgReturn(R0Process == RTR0ProcHandleSelf(), ("%p != %p\n", R0Process, RTR0ProcHandleSelf()), VERR_NOT_SUPPORTED);
    AssertMsgReturn(uAlignment == 0 || uAlignment == PAGE_SIZE, ("%d\n", uAlignment), VERR_NOT_SUPPORTED);

    PRTR0MEMOBJSOLARIS pMemToMapSolaris = (PRTR0MEMOBJSOLARIS)pMemToMap;
    size_t size = pMemToMapSolaris->Core.cb;
    void *pv = pMemToMapSolaris->Core.pv;
    pgcnt_t cPages = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;
    pgcnt_t iPage;
    uint64_t *paddrs;
    caddr_t addr;
    int rc;

    /* Create the mapping object */
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_MAPPING, pv, size);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    paddrs = kmem_zalloc(sizeof(uint64_t) * cPages, KM_SLEEP);
    for (iPage = 0; iPage < cPages; iPage++)
    {
        paddrs[iPage] = vbi_va_to_pa(pv);
        if (paddrs[iPage] == -(uint64_t)1)
        {
            cmn_err(CE_NOTE, "rtR0MemObjNativeMapUser: no page to map.\n");
            rc = VERR_MAP_FAILED;
            goto done;
        }
        pv = (void *)((uintptr_t)pv + PAGE_SIZE);
    }

    rc = vbi_user_map(&addr, fProt, paddrs, size);
    if (rc != 0)
    {
        cmn_err(CE_NOTE, "rtR0MemObjNativeMapUser: vbi failure.\n");
        rc = VERR_MAP_FAILED;
        rtR0MemObjDelete(&pMemSolaris->Core);
        goto done;
    }
    else
        rc = VINF_SUCCESS;

    pMemSolaris->Core.u.Mapping.R0Process = (RTR0PROCESS)vbi_proc();
    pMemSolaris->Core.pv = addr;
    *ppMem = &pMemSolaris->Core;
done:
    kmem_free(paddrs, sizeof(uint64_t) * cPages);
    return rc;
}


RTHCPHYS rtR0MemObjNativeGetPagePhysAddr(PRTR0MEMOBJINTERNAL pMem, size_t iPage)
{
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)pMem;

    switch (pMemSolaris->Core.enmType)
    {
        case RTR0MEMOBJTYPE_PAGE:
        case RTR0MEMOBJTYPE_LOW:
        case RTR0MEMOBJTYPE_MAPPING:
        case RTR0MEMOBJTYPE_LOCK:
        {
            uint8_t *pb = (uint8_t *)pMemSolaris->Core.pv + ((size_t)iPage << PAGE_SHIFT);
            return vbi_va_to_pa(pb);
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

