/* $Revision$ */
/** @file
 * Incredibly Portable Runtime - Ring-0 Memory Objects, Common Code.
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
#define LOG_GROUP RTLOGGROUP_DEFAULT ///@todo RTLOGGROUP_MEM
#include <iprt/memobj.h>
#include <iprt/alloc.h>
#include <iprt/process.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/log.h>
#include <iprt/param.h>
#include "internal/memobj.h"


/**
 * Internal function for allocating a new memory object.
 *
 * @returns The allocated and initialized handle.
 * @param   cbSelf      The size of the memory object handle. 0 mean default size.
 * @param   enmType     The memory object type.
 * @param   pv          The memory object mapping.
 * @param   cb          The size of the memory object.
 */
PRTR0MEMOBJINTERNAL rtR0MemObjNew(size_t cbSelf, RTR0MEMOBJTYPE enmType, void *pv, size_t cb)
{
    PRTR0MEMOBJINTERNAL pNew;

    /* validate the size */
    if (!cbSelf)
        cbSelf = sizeof(*pNew);
    Assert(cbSelf >= sizeof(*pNew));
    Assert(cbSelf == (uint32_t)cbSelf);

    /*
     * Allocate and initialize the object.
     */
    pNew = (PRTR0MEMOBJINTERNAL)RTMemAllocZ(cbSelf);
    if (pNew)
    {
        pNew->u32Magic  = RTR0MEMOBJ_MAGIC;
        pNew->cbSelf    = (uint32_t)cbSelf;
        pNew->enmType   = enmType;
        pNew->cb        = cb;
        pNew->pv        = pv;
    }
    return pNew;
}


/**
 * Deletes an incomplete memory object.
 *
 * This is for cleaning up after failures during object creation.
 *
 * @param   pMem    The incomplete memory object to delete.
 */
void rtR0MemObjDelete(PRTR0MEMOBJINTERNAL pMem)
{
    if (pMem)
    {
        pMem->u32Magic++;
        pMem->enmType = RTR0MEMOBJTYPE_END;
        RTMemFree(pMem);
    }
}


/**
 * Links a mapping object to a primary object.
 *
 * @returns IPRT status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VINF_NO_MEMORY if we couldn't expand the mapping array of the parent.
 * @param   pParent     The parent (primary) memory object.
 * @param   pChild      The child (mapping) memory object.
 */
static int rtR0MemObjLink(PRTR0MEMOBJINTERNAL pParent, PRTR0MEMOBJINTERNAL pChild)
{
    uint32_t i;

    /* sanity */
    Assert(rtR0MemObjIsMapping(pChild));
    Assert(!rtR0MemObjIsMapping(pParent));

    /* expand the array? */
    i = pParent->uRel.Parent.cMappings;
    if (i >= pParent->uRel.Parent.cMappingsAllocated)
    {
        void *pv = RTMemRealloc(pParent->uRel.Parent.papMappings,
                                (i + 32) * sizeof(pParent->uRel.Parent.papMappings[0]));
        if (!pv)
            return VERR_NO_MEMORY;
        pParent->uRel.Parent.papMappings = (PPRTR0MEMOBJINTERNAL)pv;
        pParent->uRel.Parent.cMappingsAllocated = i + 32;
        Assert(i == pParent->uRel.Parent.cMappings);
    }

    /* do the linking. */
    pParent->uRel.Parent.papMappings[i] = pChild;
    pParent->uRel.Parent.cMappings++;
    pChild->uRel.Child.pParent = pParent;

    return VINF_SUCCESS;
}


/**
 * Checks if this is mapping or not.
 *
 * @returns true if it's a mapping, otherwise false.
 * @param   MemObj      The ring-0 memory object handle.
 */
RTR0DECL(bool) RTR0MemObjIsMapping(RTR0MEMOBJ MemObj)
{
    /* Validate the object handle. */
    PRTR0MEMOBJINTERNAL pMem;
    AssertPtrReturn(MemObj, false);
    pMem = (PRTR0MEMOBJINTERNAL)MemObj;
    AssertMsgReturn(pMem->u32Magic == RTR0MEMOBJ_MAGIC, ("%p: %#x\n", pMem, pMem->u32Magic), false);
    AssertMsgReturn(pMem->enmType > RTR0MEMOBJTYPE_INVALID && pMem->enmType < RTR0MEMOBJTYPE_END, ("%p: %d\n", pMem, pMem->enmType), false);

    /* hand it on to the inlined worker. */
    return rtR0MemObjIsMapping(pMem);
}


/**
 * Gets the address of a ring-0 memory object.
 *
 * @returns The address of the memory object.
 * @returns NULL if the handle is invalid (asserts in strict builds) or if there isn't any mapping.
 * @param   MemObj  The ring-0 memory object handle.
 */
RTR0DECL(void *) RTR0MemObjAddress(RTR0MEMOBJ MemObj)
{
    /* Validate the object handle. */
    PRTR0MEMOBJINTERNAL pMem;
    if (RT_UNLIKELY(MemObj == NIL_RTR0MEMOBJ))
        return NULL;
    AssertPtrReturn(MemObj, NULL);
    pMem = (PRTR0MEMOBJINTERNAL)MemObj;
    AssertMsgReturn(pMem->u32Magic == RTR0MEMOBJ_MAGIC, ("%p: %#x\n", pMem, pMem->u32Magic), NULL);
    AssertMsgReturn(pMem->enmType > RTR0MEMOBJTYPE_INVALID && pMem->enmType < RTR0MEMOBJTYPE_END, ("%p: %d\n", pMem, pMem->enmType), NULL);

    /* return the mapping address. */
    return pMem->pv;
}


/**
 * Gets the ring-3 address of a ring-0 memory object.
 *
 * This only applies to ring-0 memory object with ring-3 mappings of some kind, i.e.
 * locked user memory, reserved user address space and user mappings. This API should
 * not be used on any other objects.
 *
 * @returns The address of the memory object.
 * @returns NIL_RTR3PTR if the handle is invalid or if it's not an object with a ring-3 mapping.
 *          Strict builds will assert in both cases.
 * @param   MemObj  The ring-0 memory object handle.
 */
RTR0DECL(RTR3PTR) RTR0MemObjAddressR3(RTR0MEMOBJ MemObj)
{
    PRTR0MEMOBJINTERNAL pMem;

    /* Validate the object handle. */
    if (RT_UNLIKELY(MemObj == NIL_RTR0MEMOBJ))
        return NIL_RTR3PTR;
    AssertPtrReturn(MemObj, NIL_RTR3PTR);
    pMem = (PRTR0MEMOBJINTERNAL)MemObj;
    AssertMsgReturn(pMem->u32Magic == RTR0MEMOBJ_MAGIC, ("%p: %#x\n", pMem, pMem->u32Magic), NIL_RTR3PTR);
    AssertMsgReturn(pMem->enmType > RTR0MEMOBJTYPE_INVALID && pMem->enmType < RTR0MEMOBJTYPE_END, ("%p: %d\n", pMem, pMem->enmType), NIL_RTR3PTR);
    if (RT_UNLIKELY(    (   pMem->enmType != RTR0MEMOBJTYPE_MAPPING
                         || pMem->u.Mapping.R0Process == NIL_RTR0PROCESS)
                    &&  (   pMem->enmType != RTR0MEMOBJTYPE_LOCK
                         || pMem->u.Lock.R0Process == NIL_RTR0PROCESS)
                    &&  (   pMem->enmType != RTR0MEMOBJTYPE_PHYS_NC
                         || pMem->u.Lock.R0Process == NIL_RTR0PROCESS)
                    &&  (   pMem->enmType != RTR0MEMOBJTYPE_RES_VIRT
                         || pMem->u.ResVirt.R0Process == NIL_RTR0PROCESS)))
        return NIL_RTR3PTR;

    /* return the mapping address. */
    return (RTR3PTR)pMem->pv;
}


/**
 * Gets the size of a ring-0 memory object.
 *
 * @returns The address of the memory object.
 * @returns 0 if the handle is invalid (asserts in strict builds) or if there isn't any mapping.
 * @param   MemObj  The ring-0 memory object handle.
 */
RTR0DECL(size_t) RTR0MemObjSize(RTR0MEMOBJ MemObj)
{
    PRTR0MEMOBJINTERNAL pMem;

    /* Validate the object handle. */
    if (RT_UNLIKELY(MemObj == NIL_RTR0MEMOBJ))
        return 0;
    AssertPtrReturn(MemObj, 0);
    pMem = (PRTR0MEMOBJINTERNAL)MemObj;
    AssertMsgReturn(pMem->u32Magic == RTR0MEMOBJ_MAGIC, ("%p: %#x\n", pMem, pMem->u32Magic), 0);
    AssertMsgReturn(pMem->enmType > RTR0MEMOBJTYPE_INVALID && pMem->enmType < RTR0MEMOBJTYPE_END, ("%p: %d\n", pMem, pMem->enmType), 0);

    /* return the size. */
    return pMem->cb;
}


/**
 * Get the physical address of an page in the memory object.
 *
 * @returns The physical address.
 * @returns NIL_RTHCPHYS if the object doesn't contain fixed physical pages.
 * @returns NIL_RTHCPHYS if the iPage is out of range.
 * @returns NIL_RTHCPHYS if the object handle isn't valid.
 * @param   MemObj  The ring-0 memory object handle.
 * @param   iPage   The page number within the object.
 */
RTR0DECL(RTHCPHYS) RTR0MemObjGetPagePhysAddr(RTR0MEMOBJ MemObj, size_t iPage)
{
    /* Validate the object handle. */
    PRTR0MEMOBJINTERNAL pMem;
    size_t cPages;
    AssertPtrReturn(MemObj, NIL_RTHCPHYS);
    pMem = (PRTR0MEMOBJINTERNAL)MemObj;
    AssertReturn(pMem->u32Magic == RTR0MEMOBJ_MAGIC, NIL_RTHCPHYS);
    AssertReturn(pMem->enmType > RTR0MEMOBJTYPE_INVALID && pMem->enmType < RTR0MEMOBJTYPE_END, NIL_RTHCPHYS);
    AssertMsgReturn(pMem->u32Magic == RTR0MEMOBJ_MAGIC, ("%p: %#x\n", pMem, pMem->u32Magic), NIL_RTHCPHYS);
    AssertMsgReturn(pMem->enmType > RTR0MEMOBJTYPE_INVALID && pMem->enmType < RTR0MEMOBJTYPE_END, ("%p: %d\n", pMem, pMem->enmType), NIL_RTHCPHYS);
    cPages = (pMem->cb >> PAGE_SHIFT);
    if (iPage >= cPages)
    {
        /* permit: while (RTR0MemObjGetPagePhysAddr(pMem, iPage++) != NIL_RTHCPHYS) {} */
        if (iPage == cPages)
            return NIL_RTHCPHYS;
        AssertReturn(iPage < (pMem->cb >> PAGE_SHIFT), NIL_RTHCPHYS);
    }

    /*
     * We know the address of physically contiguous allocations and mappings.
     */
    if (pMem->enmType == RTR0MEMOBJTYPE_CONT)
        return pMem->u.Cont.Phys + iPage * PAGE_SIZE;
    if (pMem->enmType == RTR0MEMOBJTYPE_PHYS)
        return pMem->u.Phys.PhysBase + iPage * PAGE_SIZE;

    /*
     * Do the job.
     */
    return rtR0MemObjNativeGetPagePhysAddr(pMem, iPage);
}


/**
 * Frees a ring-0 memory object.
 *
 * @returns IPRT status code.
 * @retval  VERR_INVALID_HANDLE if
 * @param   MemObj          The ring-0 memory object to be freed. NULL is accepted.
 * @param   fFreeMappings   Whether or not to free mappings of the object.
 */
RTR0DECL(int) RTR0MemObjFree(RTR0MEMOBJ MemObj, bool fFreeMappings)
{
    /*
     * Validate the object handle.
     */
    PRTR0MEMOBJINTERNAL pMem;
    int rc;

    if (MemObj == NIL_RTR0MEMOBJ)
        return VINF_SUCCESS;
    AssertPtrReturn(MemObj, VERR_INVALID_HANDLE);
    pMem = (PRTR0MEMOBJINTERNAL)MemObj;
    AssertReturn(pMem->u32Magic == RTR0MEMOBJ_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(pMem->enmType > RTR0MEMOBJTYPE_INVALID && pMem->enmType < RTR0MEMOBJTYPE_END, VERR_INVALID_HANDLE);

    /*
     * Deal with mapings according to fFreeMappings.
     */
    if (    !rtR0MemObjIsMapping(pMem)
        &&  pMem->uRel.Parent.cMappings > 0)
    {
        /* fail if not requested to free mappings. */
        if (!fFreeMappings)
            return VERR_MEMORY_BUSY;

        while (pMem->uRel.Parent.cMappings > 0)
        {
            PRTR0MEMOBJINTERNAL pChild = pMem->uRel.Parent.papMappings[--pMem->uRel.Parent.cMappings];
            pMem->uRel.Parent.papMappings[pMem->uRel.Parent.cMappings] = NULL;

            /* sanity checks. */
            AssertPtr(pChild);
            AssertFatal(pChild->u32Magic == RTR0MEMOBJ_MAGIC);
            AssertFatal(pChild->enmType > RTR0MEMOBJTYPE_INVALID && pChild->enmType < RTR0MEMOBJTYPE_END);
            AssertFatal(rtR0MemObjIsMapping(pChild));

            /* free the mapping. */
            rc = rtR0MemObjNativeFree(pChild);
            if (RT_FAILURE(rc))
            {
                Log(("RTR0MemObjFree: failed to free mapping %p: %p %#zx; rc=%Vrc\n", pChild, pChild->pv, pChild->cb, rc));
                pMem->uRel.Parent.papMappings[pMem->uRel.Parent.cMappings++] = pChild;
                return rc;
            }
        }
    }

    /*
     * Free this object.
     */
    rc = rtR0MemObjNativeFree(pMem);
    if (RT_SUCCESS(rc))
    {
        /*
         * Ok, it was freed just fine. Now, if it's a mapping we'll have to remove it from the parent.
         */
        if (rtR0MemObjIsMapping(pMem))
        {
            PRTR0MEMOBJINTERNAL pParent = pMem->uRel.Child.pParent;
            uint32_t i;

            /* sanity checks */
            AssertPtr(pParent);
            AssertFatal(pParent->u32Magic == RTR0MEMOBJ_MAGIC);
            AssertFatal(pParent->enmType > RTR0MEMOBJTYPE_INVALID && pParent->enmType < RTR0MEMOBJTYPE_END);
            AssertFatal(!rtR0MemObjIsMapping(pParent));
            AssertFatal(pParent->uRel.Parent.cMappings > 0);
            AssertPtr(pParent->uRel.Parent.papMappings);

            /* locate and remove from the array of mappings. */
            i = pParent->uRel.Parent.cMappings;
            while (i-- > 0)
            {
                if (pParent->uRel.Parent.papMappings[i] == pMem)
                {
                    pParent->uRel.Parent.papMappings[i] = pParent->uRel.Parent.papMappings[--pParent->uRel.Parent.cMappings];
                    break;
                }
            }
            Assert(i != UINT32_MAX);
        }
        else
            Assert(pMem->uRel.Parent.cMappings == 0);

        /*
         * Finally, destroy the handle.
         */
        pMem->u32Magic++;
        pMem->enmType = RTR0MEMOBJTYPE_END;
        if (!rtR0MemObjIsMapping(pMem))
            RTMemFree(pMem->uRel.Parent.papMappings);
        RTMemFree(pMem);
    }
    else
        Log(("RTR0MemObjFree: failed to free %p: %d %p %#zx; rc=%Vrc\n",
             pMem, pMem->enmType, pMem->pv, pMem->cb, rc));
    return rc;
}



/**
 * Allocates page aligned virtual kernel memory.
 *
 * The memory is taken from a non paged (= fixed physical memory backing) pool.
 *
 * @returns IPRT status code.
 * @param   pMemObj         Where to store the ring-0 memory object handle.
 * @param   cb              Number of bytes to allocate. This is rounded up to nearest page.
 * @param   fExecutable     Flag indicating whether it should be permitted to executed code in the memory object.
 */
RTR0DECL(int) RTR0MemObjAllocPage(PRTR0MEMOBJ pMemObj, size_t cb, bool fExecutable)
{
    /* sanity checks. */
    const size_t cbAligned = RT_ALIGN_Z(cb, PAGE_SIZE);
    AssertPtrReturn(pMemObj, VERR_INVALID_POINTER);
    *pMemObj = NIL_RTR0MEMOBJ;
    AssertReturn(cb > 0, VERR_INVALID_PARAMETER);
    AssertReturn(cb <= cbAligned, VERR_INVALID_PARAMETER);

    /* do the allocation. */
    return rtR0MemObjNativeAllocPage(pMemObj, cbAligned, fExecutable);
}


/**
 * Allocates page aligned virtual kernel memory with physical backing below 4GB.
 *
 * The physical memory backing the allocation is fixed.
 *
 * @returns IPRT status code.
 * @param   pMemObj         Where to store the ring-0 memory object handle.
 * @param   cb              Number of bytes to allocate. This is rounded up to nearest page.
 * @param   fExecutable     Flag indicating whether it should be permitted to executed code in the memory object.
 */
RTR0DECL(int) RTR0MemObjAllocLow(PRTR0MEMOBJ pMemObj, size_t cb, bool fExecutable)
{
    /* sanity checks. */
    const size_t cbAligned = RT_ALIGN_Z(cb, PAGE_SIZE);
    AssertPtrReturn(pMemObj, VERR_INVALID_POINTER);
    *pMemObj = NIL_RTR0MEMOBJ;
    AssertReturn(cb > 0, VERR_INVALID_PARAMETER);
    AssertReturn(cb <= cbAligned, VERR_INVALID_PARAMETER);

    /* do the allocation. */
    return rtR0MemObjNativeAllocLow(pMemObj, cbAligned, fExecutable);
}


/**
 * Allocates page aligned virtual kernel memory with contiguous physical backing below 4GB.
 *
 * The physical memory backing the allocation is fixed.
 *
 * @returns IPRT status code.
 * @param   pMemObj         Where to store the ring-0 memory object handle.
 * @param   cb              Number of bytes to allocate. This is rounded up to nearest page.
 * @param   fExecutable     Flag indicating whether it should be permitted to executed code in the memory object.
 */
RTR0DECL(int) RTR0MemObjAllocCont(PRTR0MEMOBJ pMemObj, size_t cb, bool fExecutable)
{
    /* sanity checks. */
    const size_t cbAligned = RT_ALIGN_Z(cb, PAGE_SIZE);
    AssertPtrReturn(pMemObj, VERR_INVALID_POINTER);
    *pMemObj = NIL_RTR0MEMOBJ;
    AssertReturn(cb > 0, VERR_INVALID_PARAMETER);
    AssertReturn(cb <= cbAligned, VERR_INVALID_PARAMETER);

    /* do the allocation. */
    return rtR0MemObjNativeAllocCont(pMemObj, cbAligned, fExecutable);
}


/**
 * Locks a range of user virtual memory.
 *
 * @returns IPRT status code.
 * @param   pMemObj         Where to store the ring-0 memory object handle.
 * @param   R3Ptr           User virtual address. This is rounded down to a page boundrary.
 * @param   cb              Number of bytes to lock. This is rounded up to nearest page boundrary.
 * @param   R0Process       The process to lock pages in. NIL_R0PROCESS is an alias for the current one.
 *
 * @remark  RTR0MemGetAddressR3() and RTR0MemGetAddress() will return the rounded down address.
 */
RTR0DECL(int) RTR0MemObjLockUser(PRTR0MEMOBJ pMemObj, RTR3PTR R3Ptr, size_t cb, RTR0PROCESS R0Process)
{
    /* sanity checks. */
    const size_t cbAligned = RT_ALIGN_Z(cb + (R3Ptr & PAGE_OFFSET_MASK), PAGE_SIZE);
    RTR3PTR const R3PtrAligned = (R3Ptr & ~(RTR3PTR)PAGE_OFFSET_MASK);
    AssertPtrReturn(pMemObj, VERR_INVALID_POINTER);
    *pMemObj = NIL_RTR0MEMOBJ;
    AssertReturn(cb > 0, VERR_INVALID_PARAMETER);
    AssertReturn(cb <= cbAligned, VERR_INVALID_PARAMETER);
    if (R0Process == NIL_RTR0PROCESS)
        R0Process = RTR0ProcHandleSelf();

    /* do the locking. */
    return rtR0MemObjNativeLockUser(pMemObj, R3PtrAligned, cbAligned, R0Process);
}


/**
 * Locks a range of kernel virtual memory.
 *
 * @returns IPRT status code.
 * @param   pMemObj         Where to store the ring-0 memory object handle.
 * @param   pv              Kernel virtual address. This is rounded down to a page boundrary.
 * @param   cb              Number of bytes to lock. This is rounded up to nearest page boundrary.
 *
 * @remark  RTR0MemGetAddress() will return the rounded down address.
 */
RTR0DECL(int) RTR0MemObjLockKernel(PRTR0MEMOBJ pMemObj, void *pv, size_t cb)
{
    /* sanity checks. */
    const size_t cbAligned = RT_ALIGN_Z(cb + ((uintptr_t)pv & PAGE_OFFSET_MASK), PAGE_SIZE);
    void * const pvAligned = (void *)((uintptr_t)pv & ~(uintptr_t)PAGE_OFFSET_MASK);
    AssertPtrReturn(pMemObj, VERR_INVALID_POINTER);
    *pMemObj = NIL_RTR0MEMOBJ;
    AssertReturn(cb > 0, VERR_INVALID_PARAMETER);
    AssertReturn(cb <= cbAligned, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pvAligned, VERR_INVALID_POINTER);

    /* do the allocation. */
    return rtR0MemObjNativeLockKernel(pMemObj, pvAligned, cbAligned);
}


/**
 * Allocates contiguous page aligned physical memory without (necessarily) any kernel mapping.
 *
 * @returns IPRT status code.
 * @param   pMemObj         Where to store the ring-0 memory object handle.
 * @param   cb              Number of bytes to allocate. This is rounded up to nearest page.
 * @param   PhysHighest     The highest permittable address (inclusive).
 *                          Pass NIL_RTHCPHYS if any address is acceptable.
 */
RTR0DECL(int) RTR0MemObjAllocPhys(PRTR0MEMOBJ pMemObj, size_t cb, RTHCPHYS PhysHighest)
{
    /* sanity checks. */
    const size_t cbAligned = RT_ALIGN_Z(cb, PAGE_SIZE);
    AssertPtrReturn(pMemObj, VERR_INVALID_POINTER);
    *pMemObj = NIL_RTR0MEMOBJ;
    AssertReturn(cb > 0, VERR_INVALID_PARAMETER);
    AssertReturn(cb <= cbAligned, VERR_INVALID_PARAMETER);
    AssertReturn(PhysHighest >= cb, VERR_INVALID_PARAMETER);

    /* do the allocation. */
    return rtR0MemObjNativeAllocPhys(pMemObj, cbAligned, PhysHighest);
}


/**
 * Allocates non-contiguous page aligned physical memory without (necessarily) any kernel mapping.
 *
 * @returns IPRT status code.
 * @param   pMemObj         Where to store the ring-0 memory object handle.
 * @param   cb              Number of bytes to allocate. This is rounded up to nearest page.
 * @param   PhysHighest     The highest permittable address (inclusive).
 *                          Pass NIL_RTHCPHYS if any address is acceptable.
 */
RTR0DECL(int) RTR0MemObjAllocPhysNC(PRTR0MEMOBJ pMemObj, size_t cb, RTHCPHYS PhysHighest)
{
    /* sanity checks. */
    const size_t cbAligned = RT_ALIGN_Z(cb, PAGE_SIZE);
    AssertPtrReturn(pMemObj, VERR_INVALID_POINTER);
    *pMemObj = NIL_RTR0MEMOBJ;
    AssertReturn(cb > 0, VERR_INVALID_PARAMETER);
    AssertReturn(cb <= cbAligned, VERR_INVALID_PARAMETER);
    AssertReturn(PhysHighest >= cb, VERR_INVALID_PARAMETER);

    /* do the allocation. */
    return rtR0MemObjNativeAllocPhysNC(pMemObj, cbAligned, PhysHighest);
}


/**
 * Creates a page aligned, contiguous, physical memory object.
 *
 * No physical memory is allocated, we trust you do know what you're doing.
 *
 * @returns IPRT status code.
 * @param   pMemObj         Where to store the ring-0 memory object handle.
 * @param   Phys            The physical address to start at. This is rounded down to the
 *                          nearest page boundrary.
 * @param   cb              The size of the object in bytes. This is rounded up to nearest page boundrary.
 */
RTR0DECL(int) RTR0MemObjEnterPhys(PRTR0MEMOBJ pMemObj, RTHCPHYS Phys, size_t cb)
{
    /* sanity checks. */
    const size_t cbAligned = RT_ALIGN_Z(cb + (Phys & PAGE_OFFSET_MASK), PAGE_SIZE);
    const RTHCPHYS PhysAligned = Phys & ~(RTHCPHYS)PAGE_OFFSET_MASK;
    AssertPtrReturn(pMemObj, VERR_INVALID_POINTER);
    *pMemObj = NIL_RTR0MEMOBJ;
    AssertReturn(cb > 0, VERR_INVALID_PARAMETER);
    AssertReturn(cb <= cbAligned, VERR_INVALID_PARAMETER);
    AssertReturn(Phys != NIL_RTHCPHYS, VERR_INVALID_PARAMETER);

    /* do the allocation. */
    return rtR0MemObjNativeEnterPhys(pMemObj, PhysAligned, cbAligned);
}


/**
 * Reserves kernel virtual address space.
 *
 * @returns IPRT status code.
 * @param   pMemObj         Where to store the ring-0 memory object handle.
 * @param   pvFixed         Requested address. (void *)-1 means any address. This must match the alignment.
 * @param   cb              The number of bytes to reserve. This is rounded up to nearest page.
 * @param   uAlignment      The alignment of the reserved memory.
 *                          Supported values are 0 (alias for PAGE_SIZE), PAGE_SIZE, _2M and _4M.
 */
RTR0DECL(int) RTR0MemObjReserveKernel(PRTR0MEMOBJ pMemObj, void *pvFixed, size_t cb, size_t uAlignment)
{
    /* sanity checks. */
    const size_t cbAligned = RT_ALIGN_Z(cb, PAGE_SIZE);
    AssertPtrReturn(pMemObj, VERR_INVALID_POINTER);
    *pMemObj = NIL_RTR0MEMOBJ;
    if (uAlignment == 0)
        uAlignment = PAGE_SIZE;
    AssertReturn(uAlignment == PAGE_SIZE || uAlignment == _2M || uAlignment == _4M, VERR_INVALID_PARAMETER);
    AssertReturn(cb > 0, VERR_INVALID_PARAMETER);
    AssertReturn(cb <= cbAligned, VERR_INVALID_PARAMETER);
    if (pvFixed != (void *)-1)
        AssertReturn(!((uintptr_t)pvFixed & (uAlignment - 1)), VERR_INVALID_PARAMETER);

    /* do the reservation. */
    return rtR0MemObjNativeReserveKernel(pMemObj, pvFixed, cbAligned, uAlignment);
}


/**
 * Reserves user virtual address space in the current process.
 *
 * @returns IPRT status code.
 * @param   pMemObj         Where to store the ring-0 memory object handle.
 * @param   R3PtrFixed      Requested address. (RTR3PTR)-1 means any address. This must match the alignment.
 * @param   cb              The number of bytes to reserve. This is rounded up to nearest PAGE_SIZE.
 * @param   uAlignment      The alignment of the reserved memory.
 *                          Supported values are 0 (alias for PAGE_SIZE), PAGE_SIZE, _2M and _4M.
 * @param   R0Process       The process to reserve the memory in. NIL_R0PROCESS is an alias for the current one.
 */
RTR0DECL(int) RTR0MemObjReserveUser(PRTR0MEMOBJ pMemObj, RTR3PTR R3PtrFixed, size_t cb, size_t uAlignment, RTR0PROCESS R0Process)
{
    /* sanity checks. */
    const size_t cbAligned = RT_ALIGN_Z(cb, PAGE_SIZE);
    AssertPtrReturn(pMemObj, VERR_INVALID_POINTER);
    *pMemObj = NIL_RTR0MEMOBJ;
    if (uAlignment == 0)
        uAlignment = PAGE_SIZE;
    AssertReturn(uAlignment == PAGE_SIZE || uAlignment == _2M || uAlignment == _4M, VERR_INVALID_PARAMETER);
    AssertReturn(cb > 0, VERR_INVALID_PARAMETER);
    AssertReturn(cb <= cbAligned, VERR_INVALID_PARAMETER);
    if (R3PtrFixed != (RTR3PTR)-1)
        AssertReturn(!(R3PtrFixed & (uAlignment - 1)), VERR_INVALID_PARAMETER);
    if (R0Process == NIL_RTR0PROCESS)
        R0Process = RTR0ProcHandleSelf();

    /* do the reservation. */
    return rtR0MemObjNativeReserveUser(pMemObj, R3PtrFixed, cbAligned, uAlignment, R0Process);
}


/**
 * Maps a memory object into kernel virtual address space.
 *
 * @returns IPRT status code.
 * @param   pMemObj         Where to store the ring-0 memory object handle of the mapping object.
 * @param   MemObjToMap     The object to be map.
 * @param   pvFixed         Requested address. (void *)-1 means any address. This must match the alignment.
 * @param   uAlignment      The alignment of the reserved memory.
 *                          Supported values are 0 (alias for PAGE_SIZE), PAGE_SIZE, _2M and _4M.
 * @param   fProt           Combination of RTMEM_PROT_* flags (except RTMEM_PROT_NONE).
 */
RTR0DECL(int) RTR0MemObjMapKernel(PRTR0MEMOBJ pMemObj, RTR0MEMOBJ MemObjToMap, void *pvFixed, size_t uAlignment, unsigned fProt)
{
    /* sanity checks. */
    PRTR0MEMOBJINTERNAL pMemToMap;
    PRTR0MEMOBJINTERNAL pNew;
    int rc;
    AssertPtrReturn(pMemObj, VERR_INVALID_POINTER);
    *pMemObj = NIL_RTR0MEMOBJ;
    AssertPtrReturn(MemObjToMap, VERR_INVALID_HANDLE);
    pMemToMap = (PRTR0MEMOBJINTERNAL)MemObjToMap;
    AssertReturn(pMemToMap->u32Magic == RTR0MEMOBJ_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(pMemToMap->enmType > RTR0MEMOBJTYPE_INVALID && pMemToMap->enmType < RTR0MEMOBJTYPE_END, VERR_INVALID_HANDLE);
    AssertReturn(!rtR0MemObjIsMapping(pMemToMap), VERR_INVALID_PARAMETER);
    AssertReturn(pMemToMap->enmType != RTR0MEMOBJTYPE_RES_VIRT, VERR_INVALID_PARAMETER);
    if (uAlignment == 0)
        uAlignment = PAGE_SIZE;
    AssertReturn(uAlignment == PAGE_SIZE || uAlignment == _2M || uAlignment == _4M, VERR_INVALID_PARAMETER);
    if (pvFixed != (void *)-1)
        AssertReturn(!((uintptr_t)pvFixed & (uAlignment - 1)), VERR_INVALID_PARAMETER);
    AssertReturn(fProt != RTMEM_PROT_NONE, VERR_INVALID_PARAMETER);
    AssertReturn(!(fProt & ~(RTMEM_PROT_READ | RTMEM_PROT_WRITE | RTMEM_PROT_EXEC)), VERR_INVALID_PARAMETER);


    /* do the mapping. */
    rc = rtR0MemObjNativeMapKernel(&pNew, pMemToMap, pvFixed, uAlignment, fProt);
    if (RT_SUCCESS(rc))
    {
        /* link it. */
        rc = rtR0MemObjLink(pMemToMap, pNew);
        if (RT_SUCCESS(rc))
            *pMemObj = pNew;
        else
        {
            /* damn, out of memory. bail out. */
            int rc2 = rtR0MemObjNativeFree(pNew);
            AssertRC(rc2);
            pNew->u32Magic++;
            pNew->enmType = RTR0MEMOBJTYPE_END;
            RTMemFree(pNew);
        }
    }

    return rc;
}


/**
 * Maps a memory object into user virtual address space in the current process.
 *
 * @returns IPRT status code.
 * @param   pMemObj         Where to store the ring-0 memory object handle of the mapping object.
 * @param   MemObjToMap     The object to be map.
 * @param   R3PtrFixed      Requested address. (RTR3PTR)-1 means any address. This must match the alignment.
 * @param   uAlignment      The alignment of the reserved memory.
 *                          Supported values are 0 (alias for PAGE_SIZE), PAGE_SIZE, _2M and _4M.
 * @param   fProt           Combination of RTMEM_PROT_* flags (except RTMEM_PROT_NONE).
 * @param   R0Process       The process to map the memory into. NIL_R0PROCESS is an alias for the current one.
 */
RTR0DECL(int) RTR0MemObjMapUser(PRTR0MEMOBJ pMemObj, RTR0MEMOBJ MemObjToMap, RTR3PTR R3PtrFixed, size_t uAlignment, unsigned fProt, RTR0PROCESS R0Process)
{
    /* sanity checks. */
    PRTR0MEMOBJINTERNAL pMemToMap;
    PRTR0MEMOBJINTERNAL pNew;
    int rc;
    AssertPtrReturn(pMemObj, VERR_INVALID_POINTER);
    pMemToMap = (PRTR0MEMOBJINTERNAL)MemObjToMap;
    *pMemObj = NIL_RTR0MEMOBJ;
    AssertPtrReturn(MemObjToMap, VERR_INVALID_HANDLE);
    AssertReturn(pMemToMap->u32Magic == RTR0MEMOBJ_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(pMemToMap->enmType > RTR0MEMOBJTYPE_INVALID && pMemToMap->enmType < RTR0MEMOBJTYPE_END, VERR_INVALID_HANDLE);
    AssertReturn(!rtR0MemObjIsMapping(pMemToMap), VERR_INVALID_PARAMETER);
    AssertReturn(pMemToMap->enmType != RTR0MEMOBJTYPE_RES_VIRT, VERR_INVALID_PARAMETER);
    if (uAlignment == 0)
        uAlignment = PAGE_SIZE;
    AssertReturn(uAlignment == PAGE_SIZE || uAlignment == _2M || uAlignment == _4M, VERR_INVALID_PARAMETER);
    if (R3PtrFixed != (RTR3PTR)-1)
        AssertReturn(!(R3PtrFixed & (uAlignment - 1)), VERR_INVALID_PARAMETER);
    AssertReturn(fProt != RTMEM_PROT_NONE, VERR_INVALID_PARAMETER);
    AssertReturn(!(fProt & ~(RTMEM_PROT_READ | RTMEM_PROT_WRITE | RTMEM_PROT_EXEC)), VERR_INVALID_PARAMETER);
    if (R0Process == NIL_RTR0PROCESS)
        R0Process = RTR0ProcHandleSelf();

    /* do the mapping. */
    rc = rtR0MemObjNativeMapUser(&pNew, pMemToMap, R3PtrFixed, uAlignment, fProt, R0Process);
    if (RT_SUCCESS(rc))
    {
        /* link it. */
        rc = rtR0MemObjLink(pMemToMap, pNew);
        if (RT_SUCCESS(rc))
            *pMemObj = pNew;
        else
        {
            /* damn, out of memory. bail out. */
            int rc2 = rtR0MemObjNativeFree(pNew);
            AssertRC(rc2);
            pNew->u32Magic++;
            pNew->enmType = RTR0MEMOBJTYPE_END;
            RTMemFree(pNew);
        }
    }

    return rc;
}

