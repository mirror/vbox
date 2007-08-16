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
#if 0
    /** Lock for the ring-3 / ring-0 pinned objectes.
     * This member might not be allocated for some object types. */
    KernVMLock_t        Lock;
    /** Array of physical pages.
     * This array can be 0 in length for some object types. */
    KernPageList_t      aPages[1];
#endif
} RTR0MEMOBJSOLARIS, *PRTR0MEMOBJSOLARIS;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/


int rtR0MemObjNativeFree(RTR0MEMOBJ pMem)
{
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)pMem;
//    int rc;

    switch (pMemSolaris->Core.enmType)
    {
        case RTR0MEMOBJTYPE_PHYS:
            if (!pMemSolaris->Core.pv)
                break;

        case RTR0MEMOBJTYPE_MAPPING:
            if (pMemSolaris->Core.u.Mapping.R0Process == NIL_RTR0PROCESS)
                break;

        case RTR0MEMOBJTYPE_PAGE:
        case RTR0MEMOBJTYPE_LOW:
        case RTR0MEMOBJTYPE_CONT:
            kmem_free(pMemSolaris->Core.pv, pMemSolaris->Core.cb);
            //rc = KernVMFree(pMemSolaris->Core.pv);
            //AssertMsg(!rc, ("rc=%d type=%d pv=%p cb=%#zx\n", rc, pMemSolaris->Core.enmType, pMemSolaris->Core.pv, pMemSolaris->Core.cb));
            break;

        case RTR0MEMOBJTYPE_LOCK:
            //rc = KernVMUnlock(&pMemSolaris->Lock);
            //AssertMsg(!rc, ("rc=%d\n", rc));
            break;

        case RTR0MEMOBJTYPE_RES_VIRT:
        default:
            AssertMsgFailed(("enmType=%d\n", pMemSolaris->Core.enmType));
            return VERR_INTERNAL_ERROR;
    }

    return VINF_SUCCESS;
}


int rtR0MemObjNativeAllocPage(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
#if 0
    int rc = VERR_NO_PAGE_MEMORY;
    void *pv = kmem_alloc(cb, KM_SLEEP);
    if (!pv)
        return rc;

    IOMemoryDescriptor *pMemDesc = IOMemoryDescriptor::withAddress((vm_address_t)pv, cb, kIODirectionInOut, kernel_task);
    if (pMemDesc)
    {
        /*
         * Create the IPRT memory object.
        */
        PRTR0MEMOBJDARWIN pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_PAGE, pv, cb);
        if (pMemSolaris)
        {
            pMemSolaris->pMemDesc = pMemDesc;
            *ppMem = &pMemSolaris->Core;
            return VINF_SUCCESS;
        }

        rc = VERR_NO_MEMORY;
        pMemDesc->release();
    }
    else
        rc = VERR_MEMOBJ_INIT_FAILED;
    IOFreeAligned(pv, cb);
#endif
    return VERR_NOT_IMPLEMENTED;
}


int rtR0MemObjNativeAllocLow(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    /* create the object */
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_LOW, NULL, cb);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    /* do the allocation, patiently */
    void* pv = kmem_alloc(cb, KM_SLEEP);
    if (!pv)
    {
        rtR0MemObjDelete(&pMemSolaris->Core);
        return VERR_NO_MEMORY;
    }

    /* associate object with allocated memory */
    pMemSolaris->Core.pv = pv;      /* This would be NULL by rtR0MemObjNew(), set it correctly */
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
}


int rtR0MemObjNativeAllocCont(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    NOREF(fExecutable);

#if 0
    /* create the object. */
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(RT_OFFSETOF(RTR0MEMOBJOS2, Lock), RTR0MEMOBJTYPE_CONT, NULL, cb);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    /* do the allocation. */
    ULONG ulPhys = ~0UL;
    int rc = KernVMAlloc(cb, VMDHA_FIXED | VMDHA_CONTIG, &pMemSolaris->Core.pv, (PPVOID)&ulPhys, NULL);
    if (!rc)
    {
        Assert(ulPhys != ~0UL);
        pMemSolaris->Core.u.Cont.Phys = ulPhys;
        *ppMem = &pMemSolaris->Core;
        return VINF_SUCCESS;
    }
    rtR0MemObjDelete(&pMemSolaris->Core);
    return RTErrConvertFromOS2(rc);
#endif
    return VERR_NOT_IMPLEMENTED;
}


int rtR0MemObjNativeAllocPhys(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, RTHCPHYS PhysHighest)
{
#if 0
    AssertMsgReturn(PhysHighest >= 16 *_1M, ("PhysHigest=%VHp\n", PhysHighest), VERR_NOT_IMPLEMENTED);

    /* create the object. */
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(RT_OFFSETOF(RTR0MEMOBJOS2, Lock), RTR0MEMOBJTYPE_PHYS, NULL, cb);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    /* do the allocation. */
    ULONG ulPhys = ~0UL;
    int rc = KernVMAlloc(cb, VMDHA_FIXED | VMDHA_CONTIG | (PhysHighest < _4G ? VMDHA_16M : 0), &pMemSolaris->Core.pv, (PPVOID)&ulPhys, NULL);
    if (!rc)
    {
        Assert(ulPhys != ~0UL);
        pMemSolaris->Core.u.Phys.fAllocated = true;
        pMemSolaris->Core.u.Phys.PhysBase = ulPhys;
        *ppMem = &pMemSolaris->Core;
        return VINF_SUCCESS;
    }
    rtR0MemObjDelete(&pMemSolaris->Core);
    return RTErrConvertFromOS2(rc);
#endif
    return VERR_NOT_IMPLEMENTED;
}


int rtR0MemObjNativeAllocPhysNC(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, RTHCPHYS PhysHighest)
{
    return VERR_NOT_IMPLEMENTED;
}


int rtR0MemObjNativeEnterPhys(PPRTR0MEMOBJINTERNAL ppMem, RTHCPHYS Phys, size_t cb)
{
#if 0
    /* create the object. */
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(RT_OFFSETOF(RTR0MEMOBJOS2, Lock), RTR0MEMOBJTYPE_PHYS, NULL, cb);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    /* there is no allocation here, right? it needs to be mapped somewhere first. */
    pMemSolaris->Core.u.Phys.fAllocated = false;
    pMemSolaris->Core.u.Phys.PhysBase = Phys;
    *ppMem = &pMemSolaris->Core;
    return VINF_SUCCESS;
#endif
    return VERR_NOT_IMPLEMENTED;
}


int rtR0MemObjNativeLockUser(PPRTR0MEMOBJINTERNAL ppMem, RTR3PTR R3Ptr, size_t cb, RTR0PROCESS R0Process)
{
#if 0
    AssertMsgReturn(R0Process == RTR0ProcHandleSelf(), ("%p != %p\n", R0Process, RTR0ProcHandleSelf()), VERR_NOT_SUPPORTED);

    /* create the object. */
    const ULONG cPages = cb >> PAGE_SHIFT;
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(RT_OFFSETOF(RTR0MEMOBJOS2, aPages[cPages]), RTR0MEMOBJTYPE_LOCK, (void *)R3Ptr, cb);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    /* lock it. */
    ULONG cPagesRet = cPages;
    int rc = KernVMLock(VMDHL_LONG | VMDHL_WRITE, (void *)R3Ptr, cb, &pMemSolaris->Lock, &pMemSolaris->aPages[0], &cPagesRet);
    if (!rc)
    {
        rtR0MemObjFixPageList(&pMemSolaris->aPages[0], cPages, cPagesRet);
        pMemSolaris->Core.u.Lock.R0Process = R0Process;
        *ppMem = &pMemSolaris->Core;
        return VINF_SUCCESS;
    }
    rtR0MemObjDelete(&pMemSolaris->Core);
    return RTErrConvertFromOS2(rc);
#endif
    return VERR_NOT_IMPLEMENTED;
}


int rtR0MemObjNativeLockKernel(PPRTR0MEMOBJINTERNAL ppMem, void *pv, size_t cb)
{
#if 0
    /* create the object. */
    const ULONG cPages = cb >> PAGE_SHIFT;
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(RT_OFFSETOF(RTR0MEMOBJOS2, aPages[cPages]), RTR0MEMOBJTYPE_LOCK, pv, cb);
    if (!pMemSolaris)
        return VERR_NO_MEMORY;

    /* lock it. */
    ULONG cPagesRet = cPages;
    int rc = KernVMLock(VMDHL_LONG | VMDHL_WRITE, pv, cb, &pMemSolaris->Lock, &pMemSolaris->aPages[0], &cPagesRet);
    if (!rc)
    {
        rtR0MemObjFixPageList(&pMemSolaris->aPages[0], cPages, cPagesRet);
        pMemSolaris->Core.u.Lock.R0Process = NIL_RTR0PROCESS;
        *ppMem = &pMemSolaris->Core;
        return VINF_SUCCESS;
    }
    rtR0MemObjDelete(&pMemSolaris->Core);
    return RTErrConvertFromOS2(rc);
#endif
    return VERR_NOT_IMPLEMENTED;
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
    AssertMsgReturn(pvFixed == (void *)-1, ("%p\n", pvFixed), VERR_NOT_SUPPORTED);

#if 0
/** @todo finish the implementation. */

    int rc;
    void *pvR0 = NULL;
    PRTR0MEMOBJSOLARIS pMemToMapOs2 = (PRTR0MEMOBJSOLARIS)pMemToMap;
    switch (pMemToMapOs2->Core.enmType)
    {
        /*
         * These has kernel mappings.
         */
        case RTR0MEMOBJTYPE_PAGE:
        case RTR0MEMOBJTYPE_LOW:
        case RTR0MEMOBJTYPE_CONT:
            pvR0 = pMemToMapOs2->Core.pv;
            break;

        case RTR0MEMOBJTYPE_PHYS:
            pvR0 = pMemToMapOs2->Core.pv;
            if (!pvR0)
            {
                /* no ring-0 mapping, so allocate a mapping in the process. */
                AssertMsgReturn(uAlignment == PAGE_SIZE, ("%#zx\n", uAlignment), VERR_NOT_SUPPORTED);
                AssertMsgReturn(fProt & RTMEM_PROT_WRITE, ("%#x\n", fProt), VERR_NOT_SUPPORTED);
                Assert(!pMemToMapOs2->Core.u.Phys.fAllocated);
                ULONG ulPhys = pMemToMapOs2->Core.u.Phys.PhysBase;
                rc = KernVMAlloc(pMemToMapOs2->Core.cb, VMDHA_PHYS, &pvR0, (PPVOID)&ulPhys, NULL);
                if (rc)
                    return RTErrConvertFromOS2(rc);
                pMemToMapOs2->Core.pv = pvR0;
            }
            break;

        case RTR0MEMOBJTYPE_LOCK:
            if (pMemToMapOs2->Core.u.Lock.R0Process != NIL_RTR0PROCESS)
                return VERR_NOT_SUPPORTED; /** @todo implement this... */
            pvR0 = pMemToMapOs2->Core.pv;
            break;

        case RTR0MEMOBJTYPE_RES_VIRT:
        case RTR0MEMOBJTYPE_MAPPING:
        default:
            AssertMsgFailed(("enmType=%d\n", pMemToMapOs2->Core.enmType));
            return VERR_INTERNAL_ERROR;
    }

    /*
     * Create a dummy mapping object for it.
     *
     * All mappings are read/write/execute in OS/2 and there isn't
     * any cache options, so sharing is ok. And the main memory object
     * isn't actually freed until all the mappings have been freed up
     * (reference counting).
     */
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(RT_OFFSETOF(RTR0MEMOBJOS2, Lock), RTR0MEMOBJTYPE_MAPPING, pvR0, pMemToMapOs2->Core.cb);
    if (pMemSolaris)
    {
        pMemSolaris->Core.u.Mapping.R0Process = NIL_RTR0PROCESS;
        *ppMem = &pMemSolaris->Core;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
#endif
    return VERR_NOT_IMPLEMENTED;
}

/*
 * Maps a memory object into user virtual address space in the current process.
 */
int rtR0MemObjNativeMapUser(PPRTR0MEMOBJINTERNAL ppMem, RTR0MEMOBJ pMemToMap, RTR3PTR R3PtrFixed, size_t uAlignment, unsigned fProt, RTR0PROCESS R0Process)
{
#if 0
    int rc = VERR_INVALID_PARAMETER;

    AssertMsgReturn(R0Process == RTR0ProcHandleSelf(), ("%p != %p\n", R0Process, RTR0ProcHandleSelf()), VERR_NOT_SUPPORTED);
    AssertMsgReturn(R3PtrFixed == (RTR3PTR)-1, ("%p\n", R3PtrFixed), VERR_NOT_SUPPORTED);

    PRTR0MEMOBJSOLARIS pMemToMapSolaris = (PRTR0MEMOBJSOLARIS)pMemToMap;
    caddr_t virtAddr = mmap(pMemToMapSolaris->Core.pv, pMemToMapSolaris->Core.cb, fProt, MAP_ANON, -1, 0);

    if (virtAddr != MAP_FAILED)
    {
        /*
         * Create the IPRT memory object.
         */
        PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)rtR0MemObjNew(sizeof(*pMemSolaris), RTR0MEMOBJTYPE_MAPPING,
                                                                        virtAddr, pMemToMapSolaris->Core.cb);
        if (pMemSolaris)
        {
            pMemSolaris->Core.u.Mapping.R0Process = R0Process;
            *ppMem = &pMemSolaris->Core;
            return VINF_SUCCESS;
        }

        munmap(virtAddr, pMemSolaris->Core.cb);
        rc = VERR_NO_MEMORY;
    }
    else
        rc = VERR_MAP_FAILED;

    return rc;
#endif
    return VERR_NOT_IMPLEMENTED;
}


RTHCPHYS rtR0MemObjNativeGetPagePhysAddr(PRTR0MEMOBJINTERNAL pMem, unsigned iPage)
{
    PRTR0MEMOBJSOLARIS pMemSolaris = (PRTR0MEMOBJSOLARIS)pMem;

    switch (pMemSolaris->Core.enmType)
    {
        case RTR0MEMOBJTYPE_PAGE:
        case RTR0MEMOBJTYPE_LOW:
        case RTR0MEMOBJTYPE_LOCK:
//            return pMemSolaris->aPages[iPage].Addr;
return NIL_RTHCPHYS;

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
