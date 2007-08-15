/* $Id$ */
/** @file
 * innotek Portable Runtime - Ring-0 Memory Objects, NT.
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
#include "the-nt-kernel.h"

#include <iprt/memobj.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/log.h>
#include <iprt/param.h>
#include <iprt/string.h>
#include <iprt/process.h>
#include "internal/memobj.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Maximum number of bytes we try to lock down in one go.
 * This is supposed to have a limit right below 256MB, but this appears
 * to actually be much lower. The values here have been determined experimentally.
 */
#ifdef RT_ARCH_X86
# define MAX_LOCK_MEM_SIZE   (32*1024*1024) /* 32MB */
#endif
#ifdef RT_ARCH_AMD64
# define MAX_LOCK_MEM_SIZE   (24*1024*1024) /* 24MB */
#endif


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * The NT version of the memory object structure.
 */
typedef struct RTR0MEMOBJNT
{
    /** The core structure. */
    RTR0MEMOBJINTERNAL  Core;
#ifndef IPRT_TARGET_NT4
    /** Used MmAllocatePagesForMdl(). */
    bool                fAllocatedPagesForMdl;
#endif
    /** The number of PMDLs (memory descriptor lists) in the array. */
    unsigned            cMdls;
    /** Array of MDL pointers. (variable size) */
    PMDL                apMdls[1];
} RTR0MEMOBJNT, *PRTR0MEMOBJNT;


int rtR0MemObjNativeFree(RTR0MEMOBJ pMem)
{
    PRTR0MEMOBJNT pMemNt = (PRTR0MEMOBJNT)pMem;

    /*
     * Deal with it on a per type basis (just as a variation).
     */
    switch (pMemNt->Core.enmType)
    {
        case RTR0MEMOBJTYPE_LOW:
#ifdef IPRT_TARGET_NT4
            if (pMemNt->fAllocatedPagesForMdl)
            {
                Assert(pMemNt->Core.pv && pMemNt->cMdls == 1 && pMemNt->apMdls[0]);
                MmUnmapLockedPages(pMemNt->Core.pv, pMemNt->apMdls[0]);
                pMemNt->Core.pv = NULL;

                MmFreePagesFromMdl(pMemNt->apMdls[0]);
                pMemNt->apMdls[0] = NULL;
                pMemNt->cMdls = 0;
                break;
            }
#endif
            /* fall thru */
        case RTR0MEMOBJTYPE_PAGE:
            Assert(pMemNt->Core.pv);
            ExFreePool(pMemNt->Core.pv);
            pMemNt->Core.pv = NULL;

            Assert(pMemNt->cMdls == 1 && pMemNt->apMdls[0]);
            IoFreeMdl(pMemNt->apMdls[0]);
            pMemNt->apMdls[0] = NULL;
            pMemNt->cMdls = 0;
            break;

        case RTR0MEMOBJTYPE_CONT:
            Assert(pMemNt->Core.pv);
            MmFreeContiguousMemory(pMemNt->Core.pv);
            pMemNt->Core.pv = NULL;

            Assert(pMemNt->cMdls == 1 && pMemNt->apMdls[0]);
            IoFreeMdl(pMemNt->apMdls[0]);
            pMemNt->apMdls[0] = NULL;
            pMemNt->cMdls = 0;
            break;

        case RTR0MEMOBJTYPE_PHYS:
        case RTR0MEMOBJTYPE_PHYS_NC:
#ifdef IPRT_TARGET_NT4
            if (pMemNt->fAllocatedPagesForMdl)
            {
                MmFreePagesFromMdl(pMemNt->apMdls[0]);
                pMemNt->apMdls[0] = NULL;
                pMemNt->cMdls = 0;
            }
#endif
            break;

        case RTR0MEMOBJTYPE_LOCK:
            for (unsigned i = 0; i < pMemNt->cMdls; i++)
            {
                MmUnlockPages(pMemNt->apMdl[i]);
                IoFreeMdl(pMemNt->apMdl[i]);
                pMemNt->apMdl[i] = NULL;
            }
            break;

        case RTR0MEMOBJTYPE_RES_VIRT:
            if (pMemNt->Core.u.ResVirt.R0Process == NIL_RTR0PROCESS)
            {
                MmMapIoSpace
            }
            else
            {
            }
            AssertMsgFailed(("RTR0MEMOBJTYPE_RES_VIRT\n"));
            return VERR_INTERNAL_ERROR;
            break;

        case RTR0MEMOBJTYPE_MAPPING:
        {
            Assert(pMemNt->Core.pv && pMemNt->cMdls == 1 && pMemNt->apMdls[0]);
            PRTR0MEMOBJNT pMemNtParent = (PRTR0MEMOBJNT)pMemNt->Core.uRel.Child.pParent;
            Assert(pMemNtParent);
            Assert(pMemNtParent->cMdls == 1 && pMemNtParent->apMdls[0]);
            MmUnmapLockedPages(pMemNt->Core.pv, pMemNtParent->apMdls);
            pMemNt->Core.pv = NULL;
            break;
        }

        default:
            AssertMsgFailed(("enmType=%d\n", pMemNt->Core.enmType));
            return VERR_INTERNAL_ERROR;
    }

    return VINF_SUCCESS;
}


int rtR0MemObjNativeAllocPage(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    /*
     * Try allocate the memory and create an MDL for them so
     * we can query the physical addresses and do mappings later
     * without running into out-of-memory conditions and similar problems.
     */
    int rc = VERR_NO_PAGE_MEMORY;
    void *pv = ExAllocatePoolWithTag(NonPagedPool, cb, IPRT_NT_POOL_TAG);
    if (pv)
    {
        PMDL pMdl = IoAllocateMdl(pv, cb, FALSE, FALSE, NULL);
        if (pMdl)
        {
            MmBuildMdlForNonPagedPool(pMdl);
            /** @todo if (fExecutable) */

            /*
             * Create the IPRT memory object.
             */
            PRTR0MEMOBJNT pMemNt = (PRTR0MEMOBJNT)rtR0MemObjNew(sizeof(*pMemNt), RTR0MEMOBJTYPE_PAGE, pv, cb);
            if (pMemNt)
            {
                pMemNt->cMdls = 1;
                pMemNt->apMdls[0] = pMdl;
                *ppMem = &pMemNt->Core;
                return VINF_SUCCESS;
            }

            rc = VERR_NO_MEMORY;
            IoFreeMdl(pMdl);
        }
        ExFreePool(pv);
    }
    return rc;
}


int rtR0MemObjNativeAllocLow(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    /*
     * Try see if we get lucky first...
     * (We could probably just assume we're lucky on NT4.)
     */
    int rc = rtR0MemObjNativeAllocPage(ppMem, cb, fExecutable);
    if (RT_SUCCESS(rc))
    {
        size_t iPage = cb >> PAGE_SHIFT;
        while (iPage-- > 0)
            if (rtR0MemObjNativeGetPagePhysAddr(*ppMem, iPage) >= _4G)
            {
                rc = VERR_NO_MEMORY;
                break;
            }
        if (RT_SUCCESS(rc))
            return rc;

        /* The following ASSUMES that rtR0MemObjNativeAllocPage returns a completed object. */
        RTR0MemObjFree(*ppMem, false);
        *ppMem = NULL;
    }

#ifndef IPRT_TARGET_NT4
    /*
     * Use MmAllocatePagesForMdl to specify the range of physical addresses we wish to use.
     */
    PHYSICAL_ADDRESS Zero;
    PHYSICAL_ADDRESS HighAddr;
    Zero.QuadPart = 0;
    High.QuadPart = _4G - 1;
    PMDL pMdl = MmAllocatePagesForMdl(Zero, HighAddr, Zero, cb);
    if (pMdl)
    {
        if (MmGetMdlByteCount(pMdl) >= cb)
        {
            __try
            {
                void *pv = MmMapLockedPagesSpecifyCache(pMdl, KernelMode, MmCached, NULL /* no base address */,
                                                        FALSE /* no bug check on failure */, NormalPagePriority);
                if (pv)
                {
                    PRTR0MEMOBJNT pMemNt = (PRTR0MEMOBJNT)rtR0MemObjNew(sizeof(*pMemNt), RTR0MEMOBJTYPE_LOW, pv, cb);
                    if (pMemNt)
                    {
                        pMemNt->fAllocatedPagesForMdl = true;
                        pMemNt->cMdls = 1;
                        pMemNt->apMdls[0] = pMdl;
                        *ppMem = &pMemNt->Core;
                        return VINF_SUCCESS;
                    }
                    MmUnmapLockedPages(pv, pMdl);
                }
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                /* nothing */
            }
        }
        MmFreePagesFromMdl(pMdl);
    }
#endif /* !IPRT_TARGET_NT4 */

    /*
     * Fall back on contiguous memory...
     */
    return rtR0MemObjNativeAllocCont(ppMem, cb, fExecutable);
}


/**
 * Internal worker for rtR0MemObjNativeAllocCont(), rtR0MemObjNativeAllocPhys()
 * and rtR0MemObjNativeAllocPhysNC() that takes a max physical address in addition
 * to what rtR0MemObjNativeAllocCont() does.
 *
 * @returns IPRT status code.
 * @param   ppMem           Where to store the pointer to the ring-0 memory object.
 * @param   cb              The size.
 * @param   fExecutable     Whether the mapping should be executable or not.
 * @param   PhysHighest     The highest physical address for the pages in allocation.
 */
static int rtR0MemObjNativeAllocContEx(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable, RTHCPHYS PhysHighest)
{
    /*
     * Allocate the memory and create an MDL for it.
     */
    PHYSICAL_ADDRESS PhysAddrHighest;
    PhysAddrHighest.QuadPart = PhysHighest;
    void *pv = MmAllocateContiguousMemory(cb, PhysAddrHighest);
    if (!pv)
        return VERR_NO_MEMORY;

    PMDL pMdl = IoAllocateMdl(pv, cb, FALSE, FALSE, NULL);
    if (pMdl)
    {
        MmBuildMdlForNonPagedPool(pMdl);
        /** @todo fExecutable */

        PRTR0MEMOBJNT pMemNt = (PRTR0MEMOBJNT)rtR0MemObjNew(sizeof(*pMemNt), RTR0MEMOBJTYPE_CONT, pv, cb);
        if (pMemNt)
        {
            pMemNt->Core.u.Cont.Phys = (RTHCPHYS)*MmGetMdlPfnArray(pMdl) << PAGE_SHIFT;
            pMemNt->cMdls = 1;
            pMemNt->apMdls[0] = pMdl;
            *ppMem = &pMemNt->Core;
            return VINF_SUCCESS;
        }

        IoFreeMdl(pMdl);
    }
    MmFreeContiguousMemory(pv);
    return VERR_NO_MEMORY;
}


int rtR0MemObjNativeAllocCont(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    return rtR0MemObjNativeAllocContEx(ppMem, cb, fExecutable, _4G-1);
}


int rtR0MemObjNativeAllocPhys(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, RTHCPHYS PhysHighest)
{
#ifndef IPRT_TARGET_NT4
    /*
     * Try and see if we're lucky and get a contiguous chunk from MmAllocatePagesForMdl.
     *
     * If the allocation is big, the chances are *probably* not very good. The current
     * max limit is kind of random.
     */
    if (cb < _128K)
    {
        PHYSICAL_ADDRESS Zero;
        PHYSICAL_ADDRESS HighAddr;
        Zero.QuadPart = 0;
        High.QuadPart = _4G - 1;
        PMDL pMdl = MmAllocatePagesForMdl(Zero, HighAddr, Zero, cb);
        if (pMdl)
        {
            if (MmGetMdlByteCount(pMdl) >= cb)
            {
                PPFN_NUMBER paPfns = MmGetMdlPfnArray(pMdl);
                PFN_NUMBER Pfn = paPfns[0] + 1;
                const size_t cPages = cb >> PAGE_SIZE;
                size_t iPage;
                for (iPage = 1; iPage < cPages; iPage++, Pfn++)
                    if (paPfns[iPage] != Pfn)
                        break;
                if (iPage >= cPages)
                {
                    PRTR0MEMOBJNT pMemNt = (PRTR0MEMOBJNT)rtR0MemObjNew(sizeof(*pMemNt), RTR0MEMOBJTYPE_PHYS, NULL, cb);
                    if (pMemNt)
                    {
                        pMemNt->Core.u.Phys.fAllocated = true;
                        pMemNt->Core.u.Phys.PhysBase = (RTHCPHYS)paPfns[0] << PAGE_SHIFT;
                        pMemNt->fAllocatedPagesForMdl = true;
                        pMemNt->cMdls = 1;
                        pMemNt->apMdls[0] = pMdl;
                        *ppMem = &pMemNt->Core;
                        return VINF_SUCCESS;
                    }
                }
            }
            MmFreePagesFromMdl(pMdl);
        }
    }
#endif

    return rtR0MemObjNativeAllocContEx(ppMem, cb, false, PhysHighest);
}


int rtR0MemObjNativeAllocPhysNC(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, RTHCPHYS PhysHighest)
{
#ifndef IPRT_TARGET_NT4
    PHYSICAL_ADDRESS Zero;
    PHYSICAL_ADDRESS HighAddr;
    Zero.QuadPart = 0;
    High.QuadPart = _4G - 1;
    PMDL pMdl = MmAllocatePagesForMdl(Zero, HighAddr, Zero, cb);
    if (pMdl)
    {
        if (MmGetMdlByteCount(pMdl) >= cb)
        {
            PRTR0MEMOBJNT pMemNt = (PRTR0MEMOBJNT)rtR0MemObjNew(sizeof(*pMemNt), RTR0MEMOBJTYPE_CONT, pv, cb);
            if (pMemNt)
            {
                pMemNt->fAllocatedPagesForMdl = true;
                pMemNt->cMdls = 1;
                pMemNt->apMdls[0] = pMdl;
                *ppMem = &pMemNt->Core;
                return VINF_SUCCESS;
            }
        }
        MmFreePagesFromMdl(pMdl);
    }
    return VERR_NO_MEMORY;
#else   /* IPRT_TARGET_NT4 */
    return VERR_NOT_SUPPORTED;
#endif  /* IPRT_TARGET_NT4 */
}


int rtR0MemObjNativeEnterPhys(PPRTR0MEMOBJINTERNAL ppMem, RTHCPHYS Phys, size_t cb)
{
    /*
     * Validate the address range and create a descriptor for it.
     */
    PFN_NUMBER Pfn = Phys >> PAGE_SHIFT;
    if (((RTHCPHYS)Pfn << PAGE_SHIFT) != Phys)
        return VERR_ADDRESS_TOO_BIG;

    /*
     * Create the IPRT memory object.
     */
    PRTR0MEMOBJNT pMemNt = (PRTR0MEMOBJNT)rtR0MemObjNew(sizeof(*pMemNt), RTR0MEMOBJTYPE_PHYS, NULL, cb);
    if (pMemNt)
    {
        pMemNt->Core.u.Phys.PhysBase = PhysAddr;
        pMemNt->Core.u.Phys.fAllocated = false;
        pMemNt->pMemDesc = pMemDesc;
        *ppMem = &pMemNt->Core;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}


/**
 * Internal worker for locking down pages.
 *
 * @return IPRT status code.
 *
 * @param ppMem     Where to store the memory object pointer.
 * @param pv        First page.
 * @param cb        Number of bytes.
 * @param Task      The task \a pv and \a cb refers to.
 */
static int rtR0MemObjNtLock(PPRTR0MEMOBJINTERNAL ppMem, void *pv, size_t cb, RTR0PROCESS R0Process)
{
    /*
     * Calc the number of MDLs we need and allocate the memory object structure.
     */
    unsigned cMdls = pMem->cb / MAX_LOCK_MEM_SIZE;
    if ((pMem->cb % MAX_LOCK_MEM_SIZE) > 0)
        cMdls++;
    PRTR0MEMOBJNT pMemNt = (PRTR0MEMOBJNT)rtR0MemObjNew(RT_OFFSETOF(RTR0MEMOBJNT, apMdls[cMdls]),
                                                        RTR0MEMOBJTYPE_LOCK, (void *)R3Ptr, cb);
    if (!pMemNt)
        return VERR_NO_MEMORY;

    /*
     * Loop locking down the sub parts of the memory.
     */
    int         rc = VINF_SUCCESS;
    size_t      cbTotal = 0;
    uint8_t    *pb = pv;
    unsigned    iMdl;
    for (iMdl = 0; iMdl < cMdls; iMdl++)
    {
        /*
         * Calc the Mdl size and allocate it.
         */
        size_t cbCur = cb - cbTotal;
        if (cbCur > MAX_LOCK_MEM_SIZE)
            cbCur = MAX_LOCK_MEM_SIZE;
        AssertMsg(cbCur, ("cbCur: 0!\n"));
        PMDL pMdl = IoAllocateMdl(pb, cbCur, FALSE, FALSE, NULL);
        if (!pMdl)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        /*
         * Lock the pages.
         */
        __try
        {
            MmProbeAndLockPages(pMdl, R0Process == NIL_RTR0PROCESS ? KernelMode : UserMode, IoModifyAccess);
            pMemNt->apMdls[iMdl] = pMdl;
            pMemNt->cMdls++;
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            IoFreeMdl(pMdl);
            rc = VERR_LOCK_FAILED;
            break;
        }

        /* next */
        cbTotal += cbCur;
        pb      += cbCur;
    }
    if (RT_SUCCESS(rc))
    {
        Assert(pMemNt->cMdls == cMdls);
        pMemNt->Core.u.Lock.R0Process = R0Process;
        *ppMem = &pMemNt->Core;
        return rc;
    }

    /*
     * We failed, perform cleanups.
     */
    while (iMdl-- > 0)
    {
        MmUnlockPages(pMemNt->apMdl[iMdl]);
        IoFreeMdl(pMemNt->apMdl[iMdl]);
        pMemNt->apMdl[iMdl] = NULL;
    }
    rtR0MemObjDelete(pMemNt);
    return SUPDRV_ERR_LOCK_FAILED;
}


int rtR0MemObjNativeLockUser(PPRTR0MEMOBJINTERNAL ppMem, RTR3PTR R3Ptr, size_t cb, RTR0PROCESS R0Process)
{
    AssertMsgReturn(R0Process == RTR0ProcHandleSelf(), ("%p != %p\n", R0Process, RTR0ProcHandleSelf()), VERR_NOT_SUPPORTED);
    /* ( Can use MmProbeAndLockProcessPages if we need to mess with other processes later.) */
    return rtR0MemObjNtLock(ppMem, (void *)R3Ptr, cb, R0Process);
}


int rtR0MemObjNativeLockKernel(PPRTR0MEMOBJINTERNAL ppMem, void *pv, size_t cb)
{
    return rtR0MemObjNtLock(ppMem, (void *)R3Ptr, cb, NIL_RTR0PROCESS);
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
    /*
     * Must have a memory descriptor.
     */
    int rc = VERR_INVALID_PARAMETER;
    PRTR0MEMOBJNT pMemToMapDarwin = (PRTR0MEMOBJNT)pMemToMap;
    if (pMemToMapDarwin->pMemDesc)
    {
        IOMemoryMap *pMemMap = pMemToMapDarwin->pMemDesc->map(kernel_task, kIOMapAnywhere,
                                                              kIOMapAnywhere | kIOMapDefaultCache);
        if (pMemMap)
        {
            IOVirtualAddress VirtAddr = pMemMap->getVirtualAddress();
            void *pv = (void *)(uintptr_t)VirtAddr;
            if ((uintptr_t)pv == VirtAddr)
            {
                /*
                 * Create the IPRT memory object.
                 */
                PRTR0MEMOBJNT pMemNt = (PRTR0MEMOBJNT)rtR0MemObjNew(sizeof(*pMemNt), RTR0MEMOBJTYPE_MAPPING,
                                                                                pv, pMemToMapDarwin->Core.cb);
                if (pMemNt)
                {
                    pMemNt->Core.u.Mapping.R0Process = NIL_RTR0PROCESS;
                    pMemNt->pMemMap = pMemMap;
                    *ppMem = &pMemNt->Core;
                    return VINF_SUCCESS;
                }

                rc = VERR_NO_MEMORY;
            }
            else
                rc = VERR_ADDRESS_TOO_BIG;
            pMemMap->release();
        }
        else
            rc = VERR_MAP_FAILED;
    }
    return rc;
}


int rtR0MemObjNativeMapUser(PPRTR0MEMOBJINTERNAL ppMem, RTR0MEMOBJ pMemToMap, RTR3PTR R3PtrFixed, size_t uAlignment, unsigned fProt, RTR0PROCESS R0Process)
{
    /*
     * Must have a memory descriptor.
     */
    int rc = VERR_INVALID_PARAMETER;
    PRTR0MEMOBJNT pMemToMapDarwin = (PRTR0MEMOBJNT)pMemToMap;
    if (pMemToMapDarwin->pMemDesc)
    {
        IOMemoryMap *pMemMap = pMemToMapDarwin->pMemDesc->map((task_t)R0Process, kIOMapAnywhere,
                                                              kIOMapAnywhere | kIOMapDefaultCache);
        if (pMemMap)
        {
            IOVirtualAddress VirtAddr = pMemMap->getVirtualAddress();
            void *pv = (void *)(uintptr_t)VirtAddr;
            if ((uintptr_t)pv == VirtAddr)
            {
                /*
                 * Create the IPRT memory object.
                 */
                PRTR0MEMOBJNT pMemNt = (PRTR0MEMOBJNT)rtR0MemObjNew(sizeof(*pMemNt), RTR0MEMOBJTYPE_MAPPING,
                                                                                pv, pMemToMapDarwin->Core.cb);
                if (pMemNt)
                {
                    pMemNt->Core.u.Mapping.R0Process = R0Process;
                    pMemNt->pMemMap = pMemMap;
                    *ppMem = &pMemNt->Core;
                    return VINF_SUCCESS;
                }

                rc = VERR_NO_MEMORY;
            }
            else
                rc = VERR_ADDRESS_TOO_BIG;
            pMemMap->release();
        }
        else
            rc = VERR_MAP_FAILED;
    }
    return rc;
}


RTHCPHYS rtR0MemObjNativeGetPagePhysAddr(PRTR0MEMOBJINTERNAL pMem, unsigned iPage)
{
    RTHCPHYS            PhysAddr;
    PRTR0MEMOBJNT   pMemNt = (PRTR0MEMOBJNT)pMem;

#ifdef USE_VM_MAP_WIRE
    /*
     * Locked memory doesn't have a memory descriptor and
     * needs to be handled differently.
     */
    if (pMemNt->Core.enmType == RTR0MEMOBJTYPE_LOCK)
    {
        ppnum_t PgNo;
        if (pMemNt->Core.u.Lock.R0Process == NIL_RTR0PROCESS)
            PgNo = pmap_find_phys(kernel_pmap, (uintptr_t)pMemNt->Core.pv + iPage * PAGE_SIZE);
        else
        {
            /*
             * From what I can tell, Apple seems to have locked up the all the
             * available interfaces that could help us obtain the pmap_t of a task
             * or vm_map_t.

             * So, we'll have to figure out where in the vm_map_t  structure it is
             * and read it our selves. ASSUMING that kernel_pmap is pointed to by
             * kernel_map->pmap, we scan kernel_map to locate the structure offset.
             * Not nice, but it will hopefully do the job in a reliable manner...
             *
             * (get_task_pmap, get_map_pmap or vm_map_pmap is what we really need btw.)
             */
            static int s_offPmap = -1;
            if (RT_UNLIKELY(s_offPmap == -1))
            {
                pmap_t const *p = (pmap_t *)kernel_map;
                pmap_t const * const pEnd = p + 64;
                for (; p < pEnd; p++)
                    if (*p == kernel_pmap)
                    {
                        s_offPmap = (uintptr_t)p - (uintptr_t)kernel_map;
                        break;
                    }
                AssertReturn(s_offPmap >= 0, NIL_RTHCPHYS);
            }
            pmap_t Pmap = *(pmap_t *)((uintptr_t)get_task_map((task_t)pMemNt->Core.u.Lock.R0Process) + s_offPmap);
            PgNo = pmap_find_phys(Pmap, (uintptr_t)pMemNt->Core.pv + iPage * PAGE_SIZE);
        }

        AssertReturn(PgNo, NIL_RTHCPHYS);
        PhysAddr = (RTHCPHYS)PgNo << PAGE_SHIFT;
        Assert((PhysAddr >> PAGE_SHIFT) == PgNo);
    }
    else
#endif /* USE_VM_MAP_WIRE */
    {
        /*
         * Get the memory descriptor.
         */
        IOMemoryDescriptor *pMemDesc = pMemNt->pMemDesc;
        if (!pMemDesc)
            pMemDesc = pMemNt->pMemMap->getMemoryDescriptor();
        AssertReturn(pMemDesc, NIL_RTHCPHYS);

        /*
         * If we've got a memory descriptor, use getPhysicalSegment64().
         */
        addr64_t Addr = pMemDesc->getPhysicalSegment64(iPage * PAGE_SIZE, NULL);
        AssertMsgReturn(Addr, ("iPage=%u\n", iPage), NIL_RTHCPHYS);
        PhysAddr = Addr;
        AssertMsgReturn(PhysAddr == Addr, ("PhysAddr=%VHp Addr=%RX64\n", PhysAddr, (uint64_t)Addr), NIL_RTHCPHYS);
    }

    return PhysAddr;
}

