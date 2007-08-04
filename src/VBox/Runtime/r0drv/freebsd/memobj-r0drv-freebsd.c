/* $Id$ */
/** @file
 * innotek Portable Runtime - Ring-0 Memory Objects, FreeBSD.
 */

/*
 * Copyright (c) 2007 knut st. osmundsen <bird-src-spam@anduin.net>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-freebsd-kernel.h"

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
 * The FreeBSD version of the memory object structure.
 */
typedef struct RTR0MEMOBJFREEBSD
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
} RTR0MEMOBJFREEBSD, *PRTR0MEMOBJFREEBSD;


MALLOC_DEFINE(M_IPRTMOBJ, "iprtmobj", "innotek Portable Runtime - R0MemObj");

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/


int rtR0MemObjNativeFree(RTR0MEMOBJ pMem)
{
    PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)pMem;
//    int rc;

    switch (pMemFreeBSD->Core.enmType)
    {
        case RTR0MEMOBJTYPE_CONT:
            contigfree(pMemFreeBSD->Core.pv, pMemFreeBSD->Core.cb, M_IPRTCONT);
            break;

#if 0
        case RTR0MEMOBJTYPE_PHYS:
            //if (!pMemFreeBSD->Core.pv)
            //    break;
            break

        case RTR0MEMOBJTYPE_MAPPING:
            //if (pMemFreeBSD->Core.u.Mapping.R0Process == NIL_RTR0PROCESS)
            //    break;
            break;
            
        case RTR0MEMOBJTYPE_PAGE:
        case RTR0MEMOBJTYPE_LOW:
            //rc = KernVMFree(pMemFreeBSD->Core.pv);
            //AssertMsg(!rc, ("rc=%d type=%d pv=%p cb=%#zx\n", rc, pMemFreeBSD->Core.enmType, pMemFreeBSD->Core.pv, pMemFreeBSD->Core.cb));
            break;

        case RTR0MEMOBJTYPE_LOCK:
            //rc = KernVMUnlock(&pMemFreeBSD->Lock);
            //AssertMsg(!rc, ("rc=%d\n", rc));
            break;

        case RTR0MEMOBJTYPE_RES_VIRT:
#endif
        default:
            AssertMsgFailed(("enmType=%d\n", pMemFreeBSD->Core.enmType));
            return VERR_INTERNAL_ERROR;
    }

    return VINF_SUCCESS;
}


int rtR0MemObjNativeAllocPage(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    /* malloc or like the linker: http://fxr.watson.org/fxr/source/kern/link_elf.c?v=RELENG62#L701 */
#if 0    
    /* create the object. */
    const ULONG cPages = cb >> PAGE_SHIFT;
    PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)rtR0MemObjNew(sizeof(*pMemFreeBSD), RTR0MEMOBJTYPE_PAGE, NULL, cb);
    if (!pMemFreeBSD)
        return VERR_NO_MEMORY;

    /* do the allocation. */
    int rc = KernVMAlloc(cb, VMDHA_FIXED, &pMemFreeBSD->Core.pv, (PPVOID)-1, NULL);
    if (!rc)
    {
        ULONG cPagesRet = cPages;
        rc = KernLinToPageList(pMemFreeBSD->Core.pv, cb, &pMemFreeBSD->aPages[0], &cPagesRet);
        if (!rc)
        {
            rtR0MemObjFixPageList(&pMemFreeBSD->aPages[0], cPages, cPagesRet);
            *ppMem = &pMemFreeBSD->Core;
            return VINF_SUCCESS;
        }
        KernVMFree(pMemFreeBSD->Core.pv);
    }
    rtR0MemObjDelete(&pMemFreeBSD->Core);
    NOREF(fExecutable);
    return RTErrConvertFromOS2(rc);
#else
    return VERR_NOT_IMPLEMENTED;
#endif
}


int rtR0MemObjNativeAllocLow(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    /* same as Alloc and hope we're lucky, make sure to verify the physical addresses */
    NOREF(fExecutable);

#if 0
  /* create the object. */
    const ULONG cPages = cb >> PAGE_SHIFT;
    PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)rtR0MemObjNew(RT_OFFSETOF(RTR0MEMOBJOS2, aPages[cPages]), RTR0MEMOBJTYPE_LOW, NULL, cb);
    if (!pMemFreeBSD)
        return VERR_NO_MEMORY;

    /* do the allocation. */
    int rc = KernVMAlloc(cb, VMDHA_FIXED, &pMemFreeBSD->Core.pv, (PPVOID)-1, NULL);
    if (!rc)
    {
        ULONG cPagesRet = cPages;
        rc = KernLinToPageList(pMemFreeBSD->Core.pv, cb, &pMemFreeBSD->aPages[0], &cPagesRet);
        if (!rc)
        {
            rtR0MemObjFixPageList(&pMemFreeBSD->aPages[0], cPages, cPagesRet);
            *ppMem = &pMemFreeBSD->Core;
            return VINF_SUCCESS;
        }
        KernVMFree(pMemFreeBSD->Core.pv);
    }
    rtR0MemObjDelete(&pMemFreeBSD->Core);
    return RTErrConvertFromOS2(rc);
#endif
    return VERR_NOT_IMPLEMENTED;
}


int rtR0MemObjNativeAllocCont(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    /* create the object. */
    PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)rtR0MemObjNew(sizeof(*pMemFreeBSD), RTR0MEMOBJTYPE_CONT, NULL, cb);
    if (!pMemFreeBSD)
        return VERR_NO_MEMORY;

    /* do the allocation. */
    pMemFreeBSD->Core.pv = contigmalloc(cb,                   /* size */
                                        M_IPRTMOBJ,           /* type */
                                        M_NOWAIT | M_ZERO,    /* flags */
                                        0,                    /* lowest physical address*/
                                        _4G-1,                /* highest physical address */
                                        PAGE_SIZE,            /* alignment. */
                                        0);                   /* boundrary */
    if (pMemFreeBSD->Core.pv)
    {
        pMemFreeBSD->Core.u.Cont.Phys = vtophys(pMemFreeBSD->Core.pv);
        *ppMem = &pMemFreeBSD->Core;
        return VINF_SUCCESS;
    }
    
    NOREF(fExecutable);
    rtR0MemObjDelete(&pMemFreeBSD->Core);
    return VERR_NO_MEMORY;
}


int rtR0MemObjNativeAllocPhys(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, RTHCPHYS PhysHighest)
{
    AssertMsgReturn(PhysHighest >= 16 *_1M, ("PhysHigest=%VHp\n", PhysHighest), VERR_NOT_IMPLEMENTED);
    
#if 0
    /* create the object. */
    PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)rtR0MemObjNew(RT_OFFSETOF(RTR0MEMOBJOS2, Lock), RTR0MEMOBJTYPE_PHYS, NULL, cb);
    if (!pMemFreeBSD)
        return VERR_NO_MEMORY;

    /* do the allocation. */
    ULONG ulPhys = ~0UL;
    int rc = KernVMAlloc(cb, VMDHA_FIXED | VMDHA_CONTIG | (PhysHighest < _4G ? VMDHA_16M : 0), &pMemFreeBSD->Core.pv, (PPVOID)&ulPhys, NULL);
    if (!rc)
    {
        Assert(ulPhys != ~0UL);
        pMemFreeBSD->Core.u.Phys.fAllocated = true;
        pMemFreeBSD->Core.u.Phys.PhysBase = ulPhys;
        *ppMem = &pMemFreeBSD->Core;
        return VINF_SUCCESS;
    }
    rtR0MemObjDelete(&pMemFreeBSD->Core);
    return RTErrConvertFromOS2(rc);
#endif
    return VERR_NOT_IMPLEMENTED;
}


int rtR0MemObjNativeEnterPhys(PPRTR0MEMOBJINTERNAL ppMem, RTHCPHYS Phys, size_t cb)
{
    /* create the object. */
    PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)rtR0MemObjNew(sizeof(*pMemFreeBSD), RTR0MEMOBJTYPE_PHYS, NULL, cb);
    if (!pMemFreeBSD)
        return VERR_NO_MEMORY;

    /* there is no allocation here, it needs to be mapped somewhere first. */
    pMemFreeBSD->Core.u.Phys.fAllocated = false;
    pMemFreeBSD->Core.u.Phys.PhysBase = Phys;
    *ppMem = &pMemFreeBSD->Core;
    return VINF_SUCCESS;
}


int rtR0MemObjNativeLockUser(PPRTR0MEMOBJINTERNAL ppMem, void *pv, size_t cb, RTR0PROCESS R0Process)
{
    /* see vslock and vsunlock */
#if 0
    AssertMsgReturn(R0Process == RTR0ProcHandleSelf(), ("%p != %p\n", R0Process, RTR0ProcHandleSelf()), VERR_NOT_SUPPORTED);

    /* create the object. */
    const ULONG cPages = cb >> PAGE_SHIFT;
    PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)rtR0MemObjNew(RT_OFFSETOF(RTR0MEMOBJOS2, aPages[cPages]), RTR0MEMOBJTYPE_LOCK, pv, cb);
    if (!pMemFreeBSD)
        return VERR_NO_MEMORY;

    /* lock it. */
    ULONG cPagesRet = cPages;
    int rc = KernVMLock(VMDHL_LONG | VMDHL_WRITE, pv, cb, &pMemFreeBSD->Lock, &pMemFreeBSD->aPages[0], &cPagesRet);
    if (!rc)
    {
        rtR0MemObjFixPageList(&pMemFreeBSD->aPages[0], cPages, cPagesRet);
        Assert(cb == pMemFreeBSD->Core.cb);
        Assert(pv == pMemFreeBSD->Core.pv);
        pMemFreeBSD->Core.u.Lock.R0Process = R0Process;
        *ppMem = &pMemFreeBSD->Core;
        return VINF_SUCCESS;
    }
    rtR0MemObjDelete(&pMemFreeBSD->Core);
    return RTErrConvertFromOS2(rc);
#endif
    return VERR_NOT_IMPLEMENTED;
}


int rtR0MemObjNativeLockKernel(PPRTR0MEMOBJINTERNAL ppMem, void *pv, size_t cb)
{
#if 0
    /* create the object. */
    const ULONG cPages = cb >> PAGE_SHIFT;
    PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)rtR0MemObjNew(RT_OFFSETOF(RTR0MEMOBJOS2, aPages[cPages]), RTR0MEMOBJTYPE_LOCK, pv, cb);
    if (!pMemFreeBSD)
        return VERR_NO_MEMORY;

    /* lock it. */
    ULONG cPagesRet = cPages;
    int rc = KernVMLock(VMDHL_LONG | VMDHL_WRITE, pv, cb, &pMemFreeBSD->Lock, &pMemFreeBSD->aPages[0], &cPagesRet);
    if (!rc)
    {
        rtR0MemObjFixPageList(&pMemFreeBSD->aPages[0], cPages, cPagesRet);
        pMemFreeBSD->Core.u.Lock.R0Process = NIL_RTR0PROCESS;
        *ppMem = &pMemFreeBSD->Core;
        return VINF_SUCCESS;
    }
    rtR0MemObjDelete(&pMemFreeBSD->Core);
    return RTErrConvertFromOS2(rc);
#endif
    return VERR_NOT_IMPLEMENTED;
}


int rtR0MemObjNativeReserveKernel(PPRTR0MEMOBJINTERNAL ppMem, void *pvFixed, size_t cb, size_t uAlignment)
{
    /* see kmem_alloc_nofault */
    return VERR_NOT_IMPLEMENTED;
}


int rtR0MemObjNativeReserveUser(PPRTR0MEMOBJINTERNAL ppMem, void *pvFixed, size_t cb, size_t uAlignment, RTR0PROCESS R0Process)
{
    /* see kmem_alloc_nofault */
    return VERR_NOT_IMPLEMENTED;
}


int rtR0MemObjNativeMapKernel(PPRTR0MEMOBJINTERNAL ppMem, RTR0MEMOBJ pMemToMap, void *pvFixed, size_t uAlignment, unsigned fProt)
{
    AssertMsgReturn(pvFixed == (void *)-1, ("%p\n", pvFixed), VERR_NOT_SUPPORTED);

/* Phys: see pmap_mapdev in i386/i386/pmap.c (http://fxr.watson.org/fxr/source/i386/i386/pmap.c?v=RELENG62#L2860) */
    
#if 0
/** @todo finish the implementation. */

    int rc;
    void *pvR0 = NULL;
    PRTR0MEMOBJFREEBSD pMemToMapOs2 = (PRTR0MEMOBJFREEBSD)pMemToMap;
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
    PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)rtR0MemObjNew(RT_OFFSETOF(RTR0MEMOBJOS2, Lock), RTR0MEMOBJTYPE_MAPPING, pvR0, pMemToMapOs2->Core.cb);
    if (pMemFreeBSD)
    {
        pMemFreeBSD->Core.u.Mapping.R0Process = NIL_RTR0PROCESS;
        *ppMem = &pMemFreeBSD->Core;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
#endif
    return VERR_NOT_IMPLEMENTED;
}


int rtR0MemObjNativeMapUser(PPRTR0MEMOBJINTERNAL ppMem, RTR0MEMOBJ pMemToMap, void *pvFixed, size_t uAlignment, unsigned fProt, RTR0PROCESS R0Process)
{
    AssertMsgReturn(R0Process == RTR0ProcHandleSelf(), ("%p != %p\n", R0Process, RTR0ProcHandleSelf()), VERR_NOT_SUPPORTED);
    AssertMsgReturn(pvFixed == (void *)-1, ("%p\n", pvFixed), VERR_NOT_SUPPORTED);

#if 0
    int rc;
    void *pvR0;
    void *pvR3 = NULL;
    PRTR0MEMOBJFREEBSD pMemToMapOs2 = (PRTR0MEMOBJFREEBSD)pMemToMap;
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
#if 0/* this is wrong. */
            if (!pvR0)
            {
                /* no ring-0 mapping, so allocate a mapping in the process. */
                AssertMsgReturn(uAlignment == PAGE_SIZE, ("%#zx\n", uAlignment), VERR_NOT_SUPPORTED);
                AssertMsgReturn(fProt & RTMEM_PROT_WRITE, ("%#x\n", fProt), VERR_NOT_SUPPORTED);
                Assert(!pMemToMapOs2->Core.u.Phys.fAllocated);
                ULONG ulPhys = pMemToMapOs2->Core.u.Phys.PhysBase;
                rc = KernVMAlloc(pMemToMapOs2->Core.cb, VMDHA_PHYS | VMDHA_PROCESS, &pvR3, (PPVOID)&ulPhys, NULL);
                if (rc)
                    return RTErrConvertFromOS2(rc);
            }
            break;
#endif 
            return VERR_NOT_SUPPORTED;

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
     * Map the ring-0 memory into the current process.
     */
    if (!pvR3)
    {
        Assert(pvR0);
        ULONG flFlags = 0;
        if (uAlignment == PAGE_SIZE)
            flFlags |= VMDHGP_4MB;
        if (fProt & RTMEM_PROT_WRITE)
            flFlags |= VMDHGP_WRITE;
        rc = RTR0Os2DHVMGlobalToProcess(flFlags, pvR0, pMemToMapOs2->Core.cb, &pvR3);
        if (rc)
            return RTErrConvertFromOS2(rc);
    }
    Assert(pvR3);

    /*
     * Create a mapping object for it.
     */
    PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)rtR0MemObjNew(RT_OFFSETOF(RTR0MEMOBJOS2, Lock), RTR0MEMOBJTYPE_MAPPING, pvR3, pMemToMapOs2->Core.cb);
    if (pMemFreeBSD)
    {
        Assert(pMemFreeBSD->Core.pv == pvR3);
        pMemFreeBSD->Core.u.Mapping.R0Process = R0Process;
        *ppMem = &pMemFreeBSD->Core;
        return VINF_SUCCESS;
    }
    KernVMFree(pvR3);
    return VERR_NO_MEMORY;
#endif
    return VERR_NOT_IMPLEMENTED;
}


RTHCPHYS rtR0MemObjNativeGetPagePhysAddr(PRTR0MEMOBJINTERNAL pMem, unsigned iPage)
{
    PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)pMem;

    switch (pMemFreeBSD->Core.enmType)
    {
        case RTR0MEMOBJTYPE_PAGE:
        case RTR0MEMOBJTYPE_LOW:
        case RTR0MEMOBJTYPE_LOCK:
//            return pMemFreeBSD->aPages[iPage].Addr;
return NIL_RTHCPHYS;

        case RTR0MEMOBJTYPE_CONT:
            return pMemFreeBSD->Core.u.Cont.Phys + (iPage << PAGE_SHIFT);

        case RTR0MEMOBJTYPE_PHYS:
            return pMemFreeBSD->Core.u.Phys.PhysBase + (iPage << PAGE_SHIFT);

        case RTR0MEMOBJTYPE_RES_VIRT:
        case RTR0MEMOBJTYPE_MAPPING:
        default:
            return NIL_RTHCPHYS;
    }
}
