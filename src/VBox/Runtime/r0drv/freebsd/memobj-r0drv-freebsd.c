/* $Id$ */
/** @file
 * IPRT - Ring-0 Memory Objects, FreeBSD.
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
    /** Type dependent data */
    union
    {
        /** Non physical memory allocations */
        struct
        {
            /** The VM object associated with the allocation. */
            vm_object_t         pObject;
        } NonPhys;
        /** Physical memory allocations */
        struct
        {
            /** Number of pages */
            uint32_t            cPages;
            /** Array of pages - variable */
            vm_page_t           apPages[1];
        } Phys;
    } u;
} RTR0MEMOBJFREEBSD, *PRTR0MEMOBJFREEBSD;


MALLOC_DEFINE(M_IPRTMOBJ, "iprtmobj", "IPRT - R0MemObj");



/**
 * Gets the virtual memory map the specified object is mapped into.
 *
 * @returns VM map handle on success, NULL if no map.
 * @param   pMem                The memory object.
 */
static vm_map_t rtR0MemObjFreeBSDGetMap(PRTR0MEMOBJINTERNAL pMem)
{
    switch (pMem->enmType)
    {
        case RTR0MEMOBJTYPE_PAGE:
        case RTR0MEMOBJTYPE_LOW:
        case RTR0MEMOBJTYPE_CONT:
            return kernel_map;

        case RTR0MEMOBJTYPE_PHYS:
        case RTR0MEMOBJTYPE_PHYS_NC:
            return NULL; /* pretend these have no mapping atm. */

        case RTR0MEMOBJTYPE_LOCK:
            return pMem->u.Lock.R0Process == NIL_RTR0PROCESS
                ? kernel_map
                : &((struct proc *)pMem->u.Lock.R0Process)->p_vmspace->vm_map;

        case RTR0MEMOBJTYPE_RES_VIRT:
            return pMem->u.ResVirt.R0Process == NIL_RTR0PROCESS
                ? kernel_map
                : &((struct proc *)pMem->u.ResVirt.R0Process)->p_vmspace->vm_map;

        case RTR0MEMOBJTYPE_MAPPING:
            return pMem->u.Mapping.R0Process == NIL_RTR0PROCESS
                ? kernel_map
                : &((struct proc *)pMem->u.Mapping.R0Process)->p_vmspace->vm_map;

        default:
            return NULL;
    }
}


DECLHIDDEN(int) rtR0MemObjNativeFree(RTR0MEMOBJ pMem)
{
    PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)pMem;
    int rc;

    switch (pMemFreeBSD->Core.enmType)
    {
        case RTR0MEMOBJTYPE_CONT:
            contigfree(pMemFreeBSD->Core.pv, pMemFreeBSD->Core.cb, M_IPRTMOBJ);
            break;

        case RTR0MEMOBJTYPE_PAGE:
        {
            rc = vm_map_remove(kernel_map,
                                (vm_offset_t)pMemFreeBSD->Core.pv,
                                (vm_offset_t)pMemFreeBSD->Core.pv + pMemFreeBSD->Core.cb);
            AssertMsg(rc == KERN_SUCCESS, ("%#x", rc));

            vm_page_lock_queues();
            for (uint32_t iPage = 0; iPage < pMemFreeBSD->u.Phys.cPages; iPage++)
            {
                vm_page_t pPage = pMemFreeBSD->u.Phys.apPages[iPage];
                vm_page_unwire(pPage, 0);
                vm_page_free(pPage);
            }
            vm_page_unlock_queues();
            break;
        }

        case RTR0MEMOBJTYPE_LOCK:
        {
            vm_map_t pMap = kernel_map;

            if (pMemFreeBSD->Core.u.Lock.R0Process != NIL_RTR0PROCESS)
                pMap = &((struct proc *)pMemFreeBSD->Core.u.Lock.R0Process)->p_vmspace->vm_map;

            rc = vm_map_unwire(pMap,
                               (vm_offset_t)pMemFreeBSD->Core.pv,
                               (vm_offset_t)pMemFreeBSD->Core.pv + pMemFreeBSD->Core.cb,
                               VM_MAP_WIRE_SYSTEM | VM_MAP_WIRE_NOHOLES);
            AssertMsg(rc == KERN_SUCCESS, ("%#x", rc));
            break;
        }

        case RTR0MEMOBJTYPE_RES_VIRT:
        {
            vm_map_t pMap = kernel_map;
            if (pMemFreeBSD->Core.u.Lock.R0Process != NIL_RTR0PROCESS)
                pMap = &((struct proc *)pMemFreeBSD->Core.u.Lock.R0Process)->p_vmspace->vm_map;
            rc = vm_map_remove(pMap,
                               (vm_offset_t)pMemFreeBSD->Core.pv,
                               (vm_offset_t)pMemFreeBSD->Core.pv + pMemFreeBSD->Core.cb);
            AssertMsg(rc == KERN_SUCCESS, ("%#x", rc));
            break;
        }

        case RTR0MEMOBJTYPE_MAPPING:
        {
            vm_map_t pMap = kernel_map;

            if (pMemFreeBSD->Core.u.Mapping.R0Process != NIL_RTR0PROCESS)
                pMap = &((struct proc *)pMemFreeBSD->Core.u.Mapping.R0Process)->p_vmspace->vm_map;

            rc = vm_map_remove(pMap,
                               (vm_offset_t)pMemFreeBSD->Core.pv,
                               (vm_offset_t)pMemFreeBSD->Core.pv + pMemFreeBSD->Core.cb);
            AssertMsg(rc == KERN_SUCCESS, ("%#x", rc));
            break;
        }

        case RTR0MEMOBJTYPE_PHYS:
        case RTR0MEMOBJTYPE_PHYS_NC:
        {
            vm_page_lock_queues();
            for (uint32_t iPage = 0; iPage < pMemFreeBSD->u.Phys.cPages; iPage++)
            {
                vm_page_t pPage = pMemFreeBSD->u.Phys.apPages[iPage];
                vm_page_unwire(pPage, 0);
                vm_page_free(pPage);
            }
            vm_page_unlock_queues();
            break;
        }

#ifdef USE_KMEM_ALLOC_ATTR
        case RTR0MEMOBJTYPE_LOW:
        {
            kmem_free(kernel_map, (vm_offset_t)pMemFreeBSD->Core.pv, pMemFreeBSD->Core.cb);
            break;
        }
#else
        case RTR0MEMOBJTYPE_LOW: /* unused */
#endif
        default:
            AssertMsgFailed(("enmType=%d\n", pMemFreeBSD->Core.enmType));
            return VERR_INTERNAL_ERROR;
    }

    return VINF_SUCCESS;
}


DECLHIDDEN(int) rtR0MemObjNativeAllocPage(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    int rc;
    size_t cPages = cb >> PAGE_SHIFT;

    /* create the object. */
    PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)rtR0MemObjNew(RT_OFFSETOF(RTR0MEMOBJFREEBSD, u.Phys.apPages[cPages]),
                                                                       RTR0MEMOBJTYPE_PAGE, NULL, cb);
    if (!pMemFreeBSD)
        return VERR_NO_MEMORY;

    pMemFreeBSD->u.Phys.cPages = cPages;

    vm_offset_t MapAddress = vm_map_min(kernel_map);
    rc = vm_map_find(kernel_map,                /* map */
                     NULL,                      /* object */
                     0,                         /* offset */
                     &MapAddress,               /* addr (IN/OUT) */
                     cb,                        /* length */
                     TRUE,                      /* find_space */
                     fExecutable                /* protection */
                     ? VM_PROT_ALL
                     : VM_PROT_RW,
                     VM_PROT_ALL,               /* max(_prot) */
                     0);                        /* cow (copy-on-write) */
    if (rc == KERN_SUCCESS)
    {
        rc = VINF_SUCCESS;

        for (size_t iPage = 0; iPage < cPages; iPage++)
        {
            vm_page_t   pPage;

            pPage = vm_page_alloc(NULL, iPage,
                                  VM_ALLOC_SYSTEM |
                                  VM_ALLOC_WIRED | VM_ALLOC_NOOBJ);

            if (!pPage)
            {
                /*
                    * Out of pages
                    * Remove already allocated pages
                    */
                while (iPage-- > 0)
                {
                    pPage = pMemFreeBSD->u.Phys.apPages[iPage];
                    vm_page_lock_queues();
                    vm_page_unwire(pPage, 0);
                    vm_page_free(pPage);
                    vm_page_unlock_queues();
                }
                rc = VERR_NO_MEMORY;
                break;
            }

            pPage->valid = VM_PAGE_BITS_ALL;
            pMemFreeBSD->u.Phys.apPages[iPage] = pPage;
        }

        if (rc == VINF_SUCCESS)
        {
            vm_offset_t AddressDst = MapAddress;

            for (size_t iPage = 0; iPage < cPages; iPage++)
            {
                vm_page_t pPage = pMemFreeBSD->u.Phys.apPages[iPage];

                MY_PMAP_ENTER(kernel_map->pmap, AddressDst, pPage,
                              fExecutable
                              ? VM_PROT_ALL
                              : VM_PROT_RW,
                              TRUE);

                AddressDst += PAGE_SIZE;
            }

            /* Store start address */
            pMemFreeBSD->Core.pv = (void *)MapAddress;
            *ppMem = &pMemFreeBSD->Core;
            return VINF_SUCCESS;
        }
    }
    rc = VERR_NO_MEMORY; /** @todo fix translation (borrow from darwin) */

    rtR0MemObjDelete(&pMemFreeBSD->Core);
    return rc;
}


DECLHIDDEN(int) rtR0MemObjNativeAllocLow(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
#ifdef USE_KMEM_ALLOC_ATTR
    /*
     * Use kmem_alloc_attr, fExectuable is not needed because the
     * memory will be executable by default
     */
    NOREF(fExecutable);

    /* create the object. */
    PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)rtR0MemObjNew(sizeof(*pMemFreeBSD), RTR0MEMOBJTYPE_LOW, NULL, cb);
    if (!pMemFreeBSD)
        return VERR_NO_MEMORY;

    pMemFreeBSD->Core.pv = (void *)kmem_alloc_attr(kernel_map,          /* Kernel */
                                                   cb,                  /* Amount */
                                                   M_ZERO,              /* Zero memory */
                                                   0,                   /* Low physical address */
                                                   _4G - PAGE_SIZE,     /* Highest physical address */
                                                   VM_MEMATTR_DEFAULT); /* Default memory attributes */
    if (!pMemFreeBSD->Core.pv)
        return VERR_NO_MEMORY;

    *ppMem = &pMemFreeBSD->Core;

    return VINF_SUCCESS;
#else
    /*
     * Try a Alloc first and see if we get luck, if not try contigmalloc.
     * Might wish to try find our own pages or something later if this
     * turns into a problemspot on AMD64 boxes.
     */
    int rc = rtR0MemObjNativeAllocPage(ppMem, cb, fExecutable);
    if (RT_SUCCESS(rc))
    {
        size_t iPage = cb >> PAGE_SHIFT;
        while (iPage-- > 0)
            if (rtR0MemObjNativeGetPagePhysAddr(*ppMem, iPage) > (_4G - PAGE_SIZE))
            {
                RTR0MemObjFree(*ppMem, false);
                *ppMem = NULL;
                rc = VERR_NO_MEMORY;
                break;
            }
    }
    if (RT_FAILURE(rc))
        rc = rtR0MemObjNativeAllocCont(ppMem, cb, fExecutable);
    return rc;
#endif
}


DECLHIDDEN(int) rtR0MemObjNativeAllocCont(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
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
                                        0);                   /* boundary */
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


static void rtR0MemObjFreeBSDPhysPageInit(vm_page_t pPage, vm_pindex_t iPage)
{
#if __FreeBSD_version <= 1000000
    pPage->wire_count = 1;
    pPage->pindex     = iPage;
    pPage->act_count  = 0;
    atomic_add_int(&cnt.v_wire_count, 1);

#if __FreeBSD_version >= 900040
    Assert(pPage->oflags & VPO_UNMANAGED != 0);
#else
    Assert(pPage->flags & PG_UNMANAGED != 0);
#endif
#endif
}


static int rtR0MemObjFreeBSDAllocPhysPages(PPRTR0MEMOBJINTERNAL ppMem, RTR0MEMOBJTYPE enmType,
                                           size_t cb,
                                           RTHCPHYS PhysHighest, size_t uAlignment,
                                           bool fContiguous)
{
    int        rc = VINF_SUCCESS;
    uint32_t   cPages = cb >> PAGE_SHIFT;
    vm_paddr_t VmPhysAddrHigh;
#if __FreeBSD_version >= 1000001
    int        pFlags = VM_ALLOC_INTERRUPT | VM_ALLOC_NOOBJ | VM_ALLOC_WIRED;
#endif

    /* create the object. */
    PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)rtR0MemObjNew(RT_OFFSETOF(RTR0MEMOBJFREEBSD, u.Phys.apPages[cPages]),
                                                                       enmType, NULL, cb);
    if (!pMemFreeBSD)
        return VERR_NO_MEMORY;

    pMemFreeBSD->u.Phys.cPages = cPages;

    if (PhysHighest != NIL_RTHCPHYS)
        VmPhysAddrHigh = PhysHighest;
    else
        VmPhysAddrHigh = ~(vm_paddr_t)0;

    if (fContiguous)
    {
#if __FreeBSD_version >= 1000001
        vm_page_t pPage = vm_page_alloc_contig(NULL, 0, pFlags, cPages, 0, VmPhysAddrHigh, uAlignment, 0, VM_MEMATTR_DEFAULT);
#else
        vm_page_t pPage = vm_phys_alloc_contig(cPages, 0, VmPhysAddrHigh, uAlignment, 0);
#endif

        if (pPage)
            for (uint32_t iPage = 0; iPage < cPages; iPage++)
            {
                rtR0MemObjFreeBSDPhysPageInit(&pPage[iPage], iPage);
                pMemFreeBSD->u.Phys.apPages[iPage] = &pPage[iPage];
            }
        else
            rc = VERR_NO_MEMORY;
    }
    else
    {
        /* Allocate page by page */
        for (uint32_t iPage = 0; iPage < cPages; iPage++)
        {
#if __FreeBSD_version >= 1000001
            vm_page_t pPage = vm_page_alloc_contig(NULL, iPage, pFlags, 1, 0, VmPhysAddrHigh, uAlignment, 0, VM_MEMATTR_DEFAULT);
#else
            vm_page_t pPage = vm_phys_alloc_contig(1, 0, VmPhysAddrHigh, uAlignment, 0);
#endif

            if (!pPage)
            {
                /* Free all allocated pages */
                while (iPage-- > 0)
                {
                    pPage = pMemFreeBSD->u.Phys.apPages[iPage];
                    vm_page_lock_queues();
                    vm_page_unwire(pPage, 0);
                    vm_page_free(pPage);
                    vm_page_unlock_queues();
                }
                rc = VERR_NO_MEMORY;
                break;
            }
            rtR0MemObjFreeBSDPhysPageInit(pPage, iPage);
            pMemFreeBSD->u.Phys.apPages[iPage] = pPage;
        }
    }

    if (RT_FAILURE(rc))
        rtR0MemObjDelete(&pMemFreeBSD->Core);
    else
    {
        if (enmType == RTR0MEMOBJTYPE_PHYS)
        {
            pMemFreeBSD->Core.u.Phys.PhysBase = VM_PAGE_TO_PHYS(pMemFreeBSD->u.Phys.apPages[0]);
            pMemFreeBSD->Core.u.Phys.fAllocated = true;
        }

        *ppMem = &pMemFreeBSD->Core;
    }

    return rc;
}


DECLHIDDEN(int) rtR0MemObjNativeAllocPhys(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, RTHCPHYS PhysHighest, size_t uAlignment)
{
#if 1
    return rtR0MemObjFreeBSDAllocPhysPages(ppMem, RTR0MEMOBJTYPE_PHYS, cb, PhysHighest, uAlignment, true);
#else
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
                                        uAlignment,           /* alignment. */
                                        0);                   /* boundary */
    if (pMemFreeBSD->Core.pv)
    {
        pMemFreeBSD->Core.u.Cont.Phys = vtophys(pMemFreeBSD->Core.pv);
        *ppMem = &pMemFreeBSD->Core;
        return VINF_SUCCESS;
    }

    rtR0MemObjDelete(&pMemFreeBSD->Core);
    return VERR_NO_MEMORY;
#endif
}


DECLHIDDEN(int) rtR0MemObjNativeAllocPhysNC(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, RTHCPHYS PhysHighest)
{
#if 1
    return rtR0MemObjFreeBSDAllocPhysPages(ppMem, RTR0MEMOBJTYPE_PHYS_NC, cb, PhysHighest, PAGE_SIZE, false);
#else
    return VERR_NOT_SUPPORTED;
#endif
}


DECLHIDDEN(int) rtR0MemObjNativeEnterPhys(PPRTR0MEMOBJINTERNAL ppMem, RTHCPHYS Phys, size_t cb, uint32_t uCachePolicy)
{
    AssertReturn(uCachePolicy == RTMEM_CACHE_POLICY_DONT_CARE, VERR_NOT_SUPPORTED);

    /* create the object. */
    PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)rtR0MemObjNew(sizeof(*pMemFreeBSD), RTR0MEMOBJTYPE_PHYS, NULL, cb);
    if (!pMemFreeBSD)
        return VERR_NO_MEMORY;

    /* there is no allocation here, it needs to be mapped somewhere first. */
    pMemFreeBSD->Core.u.Phys.fAllocated = false;
    pMemFreeBSD->Core.u.Phys.PhysBase = Phys;
    pMemFreeBSD->Core.u.Phys.uCachePolicy = uCachePolicy;
    *ppMem = &pMemFreeBSD->Core;
    return VINF_SUCCESS;
}


/**
 * Worker locking the memory in either kernel or user maps.
 */
static int rtR0MemObjNativeLockInMap(PPRTR0MEMOBJINTERNAL ppMem, vm_map_t pVmMap,
                                     vm_offset_t AddrStart, size_t cb, uint32_t fAccess,
                                     RTR0PROCESS R0Process, int fFlags)
{
    int rc;
    NOREF(fAccess);

    /* create the object. */
    PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)rtR0MemObjNew(sizeof(*pMemFreeBSD), RTR0MEMOBJTYPE_LOCK, (void *)AddrStart, cb);
    if (!pMemFreeBSD)
        return VERR_NO_MEMORY;

    /*
     * We could've used vslock here, but we don't wish to be subject to
     * resource usage restrictions, so we'll call vm_map_wire directly.
     */
    rc = vm_map_wire(pVmMap,                                         /* the map */
                     AddrStart,                                      /* start */
                     AddrStart + cb,                                 /* end */
                     fFlags);                                        /* flags */
    if (rc == KERN_SUCCESS)
    {
        pMemFreeBSD->Core.u.Lock.R0Process = R0Process;
        *ppMem = &pMemFreeBSD->Core;
        return VINF_SUCCESS;
    }
    rtR0MemObjDelete(&pMemFreeBSD->Core);
    return VERR_NO_MEMORY;/** @todo fix mach -> vbox error conversion for freebsd. */
}


DECLHIDDEN(int) rtR0MemObjNativeLockUser(PPRTR0MEMOBJINTERNAL ppMem, RTR3PTR R3Ptr, size_t cb, uint32_t fAccess, RTR0PROCESS R0Process)
{
    return rtR0MemObjNativeLockInMap(ppMem,
                                     &((struct proc *)R0Process)->p_vmspace->vm_map,
                                     (vm_offset_t)R3Ptr,
                                     cb,
                                     fAccess,
                                     R0Process,
                                     VM_MAP_WIRE_USER | VM_MAP_WIRE_NOHOLES);
}


DECLHIDDEN(int) rtR0MemObjNativeLockKernel(PPRTR0MEMOBJINTERNAL ppMem, void *pv, size_t cb, uint32_t fAccess)
{
    return rtR0MemObjNativeLockInMap(ppMem,
                                     kernel_map,
                                     (vm_offset_t)pv,
                                     cb,
                                     fAccess,
                                     NIL_RTR0PROCESS,
                                     VM_MAP_WIRE_SYSTEM | VM_MAP_WIRE_NOHOLES);
}


/**
 * Worker for the two virtual address space reservers.
 *
 * We're leaning on the examples provided by mmap and vm_mmap in vm_mmap.c here.
 */
static int rtR0MemObjNativeReserveInMap(PPRTR0MEMOBJINTERNAL ppMem, void *pvFixed, size_t cb, size_t uAlignment, RTR0PROCESS R0Process, vm_map_t pMap)
{
    int rc;

    /*
     * The pvFixed address range must be within the VM space when specified.
     */
    if (pvFixed != (void *)-1
        && (    (vm_offset_t)pvFixed      < vm_map_min(pMap)
            ||  (vm_offset_t)pvFixed + cb > vm_map_max(pMap)))
        return VERR_INVALID_PARAMETER;

    /*
     * Check that the specified alignment is supported.
     */
    if (uAlignment > PAGE_SIZE)
        return VERR_NOT_SUPPORTED;

    /*
     * Create the object.
     */
    PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)rtR0MemObjNew(sizeof(*pMemFreeBSD), RTR0MEMOBJTYPE_RES_VIRT, NULL, cb);
    if (!pMemFreeBSD)
        return VERR_NO_MEMORY;

    /*
     * Allocate an empty VM object and map it into the requested map.
     */
    pMemFreeBSD->u.NonPhys.pObject = vm_object_allocate(OBJT_DEFAULT, cb >> PAGE_SHIFT);
    if (pMemFreeBSD->u.NonPhys.pObject)
    {
        vm_offset_t MapAddress = pvFixed != (void *)-1
                               ? (vm_offset_t)pvFixed
                               : vm_map_min(pMap);
        if (pvFixed != (void *)-1)
            vm_map_remove(pMap,
                          MapAddress,
                          MapAddress + cb);

        rc = vm_map_find(pMap,                          /* map */
                         pMemFreeBSD->u.NonPhys.pObject, /* object */
                         0,                             /* offset */
                         &MapAddress,                   /* addr (IN/OUT) */
                         cb,                            /* length */
                         pvFixed == (void *)-1,         /* find_space */
                         VM_PROT_NONE,                  /* protection */
                         VM_PROT_ALL,                   /* max(_prot) ?? */
                         0);                            /* cow (copy-on-write) */
        if (rc == KERN_SUCCESS)
        {
            if (R0Process != NIL_RTR0PROCESS)
            {
                rc = vm_map_inherit(pMap,
                                    MapAddress,
                                    MapAddress + cb,
                                    VM_INHERIT_SHARE);
                AssertMsg(rc == KERN_SUCCESS, ("%#x\n", rc));
            }
            pMemFreeBSD->Core.pv = (void *)MapAddress;
            pMemFreeBSD->Core.u.ResVirt.R0Process = R0Process;
            *ppMem = &pMemFreeBSD->Core;
            return VINF_SUCCESS;
        }
        vm_object_deallocate(pMemFreeBSD->u.NonPhys.pObject);
        rc = VERR_NO_MEMORY; /** @todo fix translation (borrow from darwin) */
    }
    else
        rc = VERR_NO_MEMORY;
    rtR0MemObjDelete(&pMemFreeBSD->Core);
    return rc;

}


DECLHIDDEN(int) rtR0MemObjNativeReserveKernel(PPRTR0MEMOBJINTERNAL ppMem, void *pvFixed, size_t cb, size_t uAlignment)
{
    return rtR0MemObjNativeReserveInMap(ppMem, pvFixed, cb, uAlignment, NIL_RTR0PROCESS, kernel_map);
}


DECLHIDDEN(int) rtR0MemObjNativeReserveUser(PPRTR0MEMOBJINTERNAL ppMem, RTR3PTR R3PtrFixed, size_t cb, size_t uAlignment, RTR0PROCESS R0Process)
{
    return rtR0MemObjNativeReserveInMap(ppMem, (void *)R3PtrFixed, cb, uAlignment, R0Process,
                                        &((struct proc *)R0Process)->p_vmspace->vm_map);
}


DECLHIDDEN(int) rtR0MemObjNativeMapKernel(PPRTR0MEMOBJINTERNAL ppMem, RTR0MEMOBJ pMemToMap, void *pvFixed, size_t uAlignment,
                                          unsigned fProt, size_t offSub, size_t cbSub)
{
    AssertMsgReturn(!offSub && !cbSub, ("%#x %#x\n", offSub, cbSub), VERR_NOT_SUPPORTED);
    AssertMsgReturn(pvFixed == (void *)-1, ("%p\n", pvFixed), VERR_NOT_SUPPORTED);

    /*
     * Check that the specified alignment is supported.
     */
    if (uAlignment > PAGE_SIZE)
        return VERR_NOT_SUPPORTED;

/* Phys: see pmap_mapdev in i386/i386/pmap.c (http://fxr.watson.org/fxr/source/i386/i386/pmap.c?v=RELENG62#L2860) */
/** @todo finish the implementation. */

    return VERR_NOT_SUPPORTED;
}


/* see http://markmail.org/message/udhq33tefgtyfozs */
DECLHIDDEN(int) rtR0MemObjNativeMapUser(PPRTR0MEMOBJINTERNAL ppMem, RTR0MEMOBJ pMemToMap, RTR3PTR R3PtrFixed, size_t uAlignment, unsigned fProt, RTR0PROCESS R0Process)
{
    /*
     * Check for unsupported stuff.
     */
    AssertMsgReturn(R0Process == RTR0ProcHandleSelf(), ("%p != %p\n", R0Process, RTR0ProcHandleSelf()), VERR_NOT_SUPPORTED);
    AssertMsgReturn(R3PtrFixed == (RTR3PTR)-1, ("%p\n", R3PtrFixed), VERR_NOT_SUPPORTED);
    if (uAlignment > PAGE_SIZE)
        return VERR_NOT_SUPPORTED;

    int                rc;
    PRTR0MEMOBJFREEBSD pMemToMapFreeBSD = (PRTR0MEMOBJFREEBSD)pMemToMap;
    struct proc       *pProc            = (struct proc *)R0Process;
    struct vm_map     *pProcMap         = &pProc->p_vmspace->vm_map;

    /* calc protection */
    vm_prot_t       ProtectionFlags = 0;
    if ((fProt & RTMEM_PROT_NONE) == RTMEM_PROT_NONE)
        ProtectionFlags = VM_PROT_NONE;
    if ((fProt & RTMEM_PROT_READ) == RTMEM_PROT_READ)
        ProtectionFlags |= VM_PROT_READ;
    if ((fProt & RTMEM_PROT_WRITE) == RTMEM_PROT_WRITE)
        ProtectionFlags |= VM_PROT_WRITE;
    if ((fProt & RTMEM_PROT_EXEC) == RTMEM_PROT_EXEC)
        ProtectionFlags |= VM_PROT_EXECUTE;

    /* calc mapping address */
    PROC_LOCK(pProc);
    vm_offset_t AddrR3 = round_page((vm_offset_t)pProc->p_vmspace->vm_daddr + lim_max(pProc, RLIMIT_DATA));
    PROC_UNLOCK(pProc);

    /* Insert the object in the map. */
    rc = vm_map_find(pProcMap,              /* Map to insert the object in */
                     NULL,                  /* Object to map */
                     0,                     /* Start offset in the object */
                     &AddrR3,               /* Start address IN/OUT */
                     pMemToMap->cb,         /* Size of the mapping */
                     TRUE,                  /* Whether a suitable address should be searched for first */
                     ProtectionFlags,       /* protection flags */
                     VM_PROT_ALL,           /* Maximum protection flags */
                     0);                    /* Copy on write */

    /* Map the memory page by page into the destination map. */
    if (rc == KERN_SUCCESS)
    {
        size_t         cPages       = pMemToMap->cb >> PAGE_SHIFT;;
        pmap_t         pPhysicalMap = pProcMap->pmap;
        vm_offset_t    AddrR3Dst    = AddrR3;

        if (   pMemToMap->enmType == RTR0MEMOBJTYPE_PHYS
            || pMemToMap->enmType == RTR0MEMOBJTYPE_PHYS_NC
            || pMemToMap->enmType == RTR0MEMOBJTYPE_PAGE)
        {
            /* Mapping physical allocations */
            Assert(cPages == pMemToMapFreeBSD->u.Phys.cPages);

            /* Insert the memory page by page into the mapping. */
            for (uint32_t iPage = 0; iPage < cPages; iPage++)
            {
                vm_page_t pPage = pMemToMapFreeBSD->u.Phys.apPages[iPage];

                MY_PMAP_ENTER(pPhysicalMap, AddrR3Dst, pPage, ProtectionFlags, TRUE);
                AddrR3Dst += PAGE_SIZE;
            }
        }
        else
        {
            /* Mapping cont or low memory types */
            vm_offset_t AddrToMap = (vm_offset_t)pMemToMap->pv;

            for (uint32_t iPage = 0; iPage < cPages; iPage++)
            {
                vm_page_t pPage = PHYS_TO_VM_PAGE(vtophys(AddrToMap));

                MY_PMAP_ENTER(pPhysicalMap, AddrR3Dst, pPage, ProtectionFlags, TRUE);
                AddrR3Dst += PAGE_SIZE;
                AddrToMap += PAGE_SIZE;
            }
        }
    }

    if (RT_SUCCESS(rc))
    {
        /*
         * Create a mapping object for it.
         */
        PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)rtR0MemObjNew(sizeof(RTR0MEMOBJFREEBSD),
                                                                           RTR0MEMOBJTYPE_MAPPING,
                                                                           (void *)AddrR3,
                                                                           pMemToMap->cb);
        if (pMemFreeBSD)
        {
            Assert((vm_offset_t)pMemFreeBSD->Core.pv == AddrR3);
            pMemFreeBSD->Core.u.Mapping.R0Process = R0Process;
            *ppMem = &pMemFreeBSD->Core;
            return VINF_SUCCESS;
        }

        rc = vm_map_remove(pProcMap, ((vm_offset_t)AddrR3), ((vm_offset_t)AddrR3) + pMemToMap->cb);
        AssertMsg(rc == KERN_SUCCESS, ("Deleting mapping failed\n"));
    }

    return VERR_NO_MEMORY;
}


DECLHIDDEN(int) rtR0MemObjNativeProtect(PRTR0MEMOBJINTERNAL pMem, size_t offSub, size_t cbSub, uint32_t fProt)
{
    vm_prot_t          ProtectionFlags = 0;
    vm_offset_t        AddrStart       = (uintptr_t)pMem->pv + offSub;
    vm_offset_t        AddrEnd         = AddrStart + cbSub;
    vm_map_t           pVmMap          = rtR0MemObjFreeBSDGetMap(pMem);

    if (!pVmMap)
        return VERR_NOT_SUPPORTED;

    if ((fProt & RTMEM_PROT_NONE) == RTMEM_PROT_NONE)
        ProtectionFlags = VM_PROT_NONE;
    if ((fProt & RTMEM_PROT_READ) == RTMEM_PROT_READ)
        ProtectionFlags |= VM_PROT_READ;
    if ((fProt & RTMEM_PROT_WRITE) == RTMEM_PROT_WRITE)
        ProtectionFlags |= VM_PROT_WRITE;
    if ((fProt & RTMEM_PROT_EXEC) == RTMEM_PROT_EXEC)
        ProtectionFlags |= VM_PROT_EXECUTE;

    int krc = vm_map_protect(pVmMap, AddrStart, AddrEnd, ProtectionFlags, FALSE);
    if (krc == KERN_SUCCESS)
        return VINF_SUCCESS;

    return VERR_NOT_SUPPORTED;
}


DECLHIDDEN(RTHCPHYS) rtR0MemObjNativeGetPagePhysAddr(PRTR0MEMOBJINTERNAL pMem, size_t iPage)
{
    PRTR0MEMOBJFREEBSD pMemFreeBSD = (PRTR0MEMOBJFREEBSD)pMem;

    switch (pMemFreeBSD->Core.enmType)
    {
        case RTR0MEMOBJTYPE_LOCK:
        {
            if (    pMemFreeBSD->Core.u.Lock.R0Process != NIL_RTR0PROCESS
                &&  pMemFreeBSD->Core.u.Lock.R0Process != (RTR0PROCESS)curproc)
            {
                /* later */
                return NIL_RTHCPHYS;
            }

            vm_offset_t pb = (vm_offset_t)pMemFreeBSD->Core.pv + (iPage << PAGE_SHIFT);

            struct proc    *pProc     = (struct proc *)pMemFreeBSD->Core.u.Lock.R0Process;
            struct vm_map  *pProcMap  = &pProc->p_vmspace->vm_map;
            pmap_t pPhysicalMap       = pProcMap->pmap;

            return pmap_extract(pPhysicalMap, pb);
        }

        case RTR0MEMOBJTYPE_MAPPING:
        {
            vm_offset_t pb = (vm_offset_t)pMemFreeBSD->Core.pv + (iPage << PAGE_SHIFT);

            if (pMemFreeBSD->Core.u.Mapping.R0Process != NIL_RTR0PROCESS)
            {
                struct proc    *pProc     = (struct proc *)pMemFreeBSD->Core.u.Mapping.R0Process;
                struct vm_map  *pProcMap  = &pProc->p_vmspace->vm_map;
                pmap_t pPhysicalMap       = pProcMap->pmap;

                return pmap_extract(pPhysicalMap, pb);
            }
            return vtophys(pb);
        }

        case RTR0MEMOBJTYPE_CONT:
            return pMemFreeBSD->Core.u.Cont.Phys + (iPage << PAGE_SHIFT);

        case RTR0MEMOBJTYPE_PHYS:
            return pMemFreeBSD->Core.u.Phys.PhysBase + (iPage << PAGE_SHIFT);

        case RTR0MEMOBJTYPE_PAGE:
        case RTR0MEMOBJTYPE_PHYS_NC:
            return VM_PAGE_TO_PHYS(pMemFreeBSD->u.Phys.apPages[iPage]);

#ifdef USE_KMEM_ALLOC_ATTR
        case RTR0MEMOBJTYPE_LOW:
        {
            vm_offset_t pb = (vm_offset_t)pMemFreeBSD->Core.pv + (iPage << PAGE_SHIFT);
            return vtophys(pb);
        }
#else
        case RTR0MEMOBJTYPE_LOW:
#endif
        case RTR0MEMOBJTYPE_RES_VIRT:
        default:
            return NIL_RTHCPHYS;
    }
}

