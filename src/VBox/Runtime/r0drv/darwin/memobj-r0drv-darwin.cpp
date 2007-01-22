/* $Id$ */
/** @file
 * InnoTek Portable Runtime - Ring-0 Memory Objects, Darwin.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-darwin-kernel.h"

#include <iprt/memobj.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/log.h>
#include <iprt/param.h>
#include <iprt/string.h>
#include "internal/memobj.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * The Darwin version of the memory object structure.
 */
typedef struct RTR0MEMOBJDARWIN
{
    /** The core structure. */
    RTR0MEMOBJINTERNAL  Core;
    /** Pointer to the memory descriptor created for allocated and locked memory. */
    IOMemoryDescriptor *pMemDesc;
    /** Pointer to the memory mapping object for mapped memory. */
    IOMemoryMap        *pMemMap;
} RTR0MEMOBJDARWIN, *PRTR0MEMOBJDARWIN;


int rtR0MemObjNativeFree(RTR0MEMOBJ pMem)
{
    PRTR0MEMOBJDARWIN pMemDarwin = (PRTR0MEMOBJDARWIN)pMem;

    /*
     * Release the IOMemoryDescriptor/IOMemoryMap associated with the object.
     */
    if (pMemDarwin->pMemDesc)
    {
        if (pMemDarwin->Core.enmType == RTR0MEMOBJTYPE_LOCK)
            pMemDarwin->pMemDesc->complete();
        pMemDarwin->pMemDesc->release();
        pMemDarwin->pMemDesc = NULL;
        Assert(!pMemDarwin->pMemMap);
    }
    else if (pMemDarwin->pMemMap)
    {
        pMemDarwin->pMemMap->release();
        pMemDarwin->pMemMap = NULL;
    }

    /*
     * Release any memory that we've allocated or locked.
     */
    switch (pMemDarwin->Core.enmType)
    {
        case RTR0MEMOBJTYPE_PAGE:
            IOFreeAligned(pMemDarwin->Core.pv, pMemDarwin->Core.cb);
            break;

        /*case RTR0MEMOBJTYPE_LOW: => RTR0MEMOBJTYPE_CONT
            break;*/

        case RTR0MEMOBJTYPE_CONT:
            IOFreeContiguous(pMemDarwin->Core.pv, pMemDarwin->Core.cb);
            break;

        case RTR0MEMOBJTYPE_LOCK:
            break;

        case RTR0MEMOBJTYPE_PHYS:
            /*if (pMemDarwin->Core.u.Phys.fAllocated)
                IOFreePhysical(pMemDarwin->Core.u.Phys.PhysBase, pMemDarwin->Core.cb);*/
            Assert(!pMemDarwin->Core.u.Phys.fAllocated);
            break;

        case RTR0MEMOBJTYPE_RES_VIRT:
            AssertMsgFailed(("RTR0MEMOBJTYPE_RES_VIRT\n"));
            return VERR_INTERNAL_ERROR;
            break;

        case RTR0MEMOBJTYPE_MAPPING:
            /* nothing to do here. */
            break;

        default:
            AssertMsgFailed(("enmType=%d\n", pMemDarwin->Core.enmType));
            return VERR_INTERNAL_ERROR;
    }

    return VINF_SUCCESS;
}


int rtR0MemObjNativeAllocPage(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    /*
     * Try allocate the memory and create it's IOMemoryDescriptor first.
     */
    int rc = VERR_NO_PAGE_MEMORY;
    AssertCompile(sizeof(IOPhysicalAddress) == 4);
    void *pv = IOMallocAligned(cb, PAGE_SIZE);
    if (pv)
    {
        IOMemoryDescriptor *pMemDesc = IOMemoryDescriptor::withAddress((vm_address_t)pv, cb, kIODirectionInOut, kernel_task);
        if (pMemDesc)
        {
            /*
             * Create the IPRT memory object.
             */
            PRTR0MEMOBJDARWIN pMemDarwin = (PRTR0MEMOBJDARWIN)rtR0MemObjNew(sizeof(*pMemDarwin), RTR0MEMOBJTYPE_PAGE, pv, cb);
            if (pMemDarwin)
            {
                pMemDarwin->pMemDesc = pMemDesc;
                *ppMem = &pMemDarwin->Core;
                return VINF_SUCCESS;
            }

            rc = VERR_NO_MEMORY;
            pMemDesc->release();
        }
        else
            rc = VERR_MEMOBJ_INIT_FAILED;
        IOFreeAligned(pv, cb);
    }
    return rc;
}


int rtR0MemObjNativeAllocLow(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    /*
     * IOMallocContiguous is the most suitable API.
     */
    return rtR0MemObjNativeAllocCont(ppMem, cb, fExecutable);
}


int rtR0MemObjNativeAllocCont(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    /*
     * Try allocate the memory and create it's IOMemoryDescriptor first.
     */
    int rc = VERR_NO_CONT_MEMORY;
    AssertCompile(sizeof(IOPhysicalAddress) == 4);
    void *pv = IOMallocContiguous(cb, PAGE_SIZE, NULL);
    if (pv)
    {
        IOMemoryDescriptor *pMemDesc = IOMemoryDescriptor::withAddress((vm_address_t)pv, cb, kIODirectionInOut, kernel_task);
        if (pMemDesc)
        {
            /* a bit of useful paranoia. */
            addr64_t PhysAddr = pMemDesc->getPhysicalSegment64(0, NULL);
            Assert(PhysAddr == pMemDesc->getPhysicalAddress());
            if (    PhysAddr > 0
                &&  PhysAddr <= _4G
                &&  PhysAddr + cb <= _4G)
            {
                /*
                 * Create the IPRT memory object.
                 */
                PRTR0MEMOBJDARWIN pMemDarwin = (PRTR0MEMOBJDARWIN)rtR0MemObjNew(sizeof(*pMemDarwin), RTR0MEMOBJTYPE_CONT, pv, cb);
                if (pMemDarwin)
                {
                    pMemDarwin->Core.u.Cont.Phys = PhysAddr;
                    pMemDarwin->pMemDesc = pMemDesc;
                    *ppMem = &pMemDarwin->Core;
                    return VINF_SUCCESS;
                }

                rc = VERR_NO_MEMORY;
            }
            else
            {
                AssertMsgFailed(("PhysAddr=%llx\n", (unsigned long long)PhysAddr));
                rc = VERR_INTERNAL_ERROR;
            }
            pMemDesc->release();
        }
        else
            rc = VERR_MEMOBJ_INIT_FAILED;
        IOFreeContiguous(pv, cb);
    }
    return rc;
}


int rtR0MemObjNativeAllocPhys(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, RTHCPHYS PhysHighest)
{
#if 0 /* turned out IOMallocPhysical isn't exported yet. sigh. */
    /*
     * Try allocate the memory and create it's IOMemoryDescriptor first.
     * Note that IOMallocPhysical is not working correctly (it's ignoring the mask).
     */

    /* first calc the mask (in the hope that it'll be used) */
    IOPhysicalAddress PhysMask = ~(IOPhysicalAddress)PAGE_OFFSET_MASK;
    if (PhysHighest != NIL_RTHCPHYS)
    {
        PhysMask = ~(IOPhysicalAddress)0;
        while (PhysMask > PhysHighest)
            PhysMask >>= 1;
        AssertReturn(PhysMask + 1 < cb, VERR_INVALID_PARAMETER);
        PhysMask &= ~(IOPhysicalAddress)PAGE_OFFSET_MASK;
    }

    /* try allocate physical memory. */
    int rc = VERR_NO_PHYS_MEMORY;
    mach_vm_address_t PhysAddr64 = IOMallocPhysical(cb, PhysMask);
    if (PhysAddr64)
    {
        IOPhysicalAddress PhysAddr = PhysAddr64;
        if (    PhysAddr == PhysAddr64
            &&  PhysAddr < PhysHighest
            &&  PhysAddr + cb <= PhysHighest)
        {
            /* create a descriptor. */
            IOMemoryDescriptor *pMemDesc = IOMemoryDescriptor::withPhysicalAddress(PhysAddr, cb, kIODirectionInOut);
            if (pMemDesc)
            {
                Assert(PhysAddr == pMemDesc->getPhysicalAddress());

                /*
                 * Create the IPRT memory object.
                 */
                PRTR0MEMOBJDARWIN pMemDarwin = (PRTR0MEMOBJDARWIN)rtR0MemObjNew(sizeof(*pMemDarwin), RTR0MEMOBJTYPE_PHYS, NULL, cb);
                if (pMemDarwin)
                {
                    pMemDarwin->Core.u.Phys.PhysBase = PhysAddr;
                    pMemDarwin->Core.u.Phys.fAllocated = true;
                    pMemDarwin->pMemDesc = pMemDesc;
                    *ppMem = &pMemDarwin->Core;
                    return VINF_SUCCESS;
                }

                rc = VERR_NO_MEMORY;
                pMemDesc->release();
            }
            else
                rc = VERR_MEMOBJ_INIT_FAILED;
        }
        else
        {
            AssertMsgFailed(("PhysAddr=%#llx PhysAddr64=%#llx PhysHigest=%#llx\n", (unsigned long long)PhysAddr,
                             (unsigned long long)PhysAddr64, (unsigned long long)PhysHighest));
            rc = VERR_INTERNAL_ERROR;
        }

        IOFreePhysical(PhysAddr64, cb);
    }

    /*
     * Just in case IOMallocContigus doesn't work right, we can try fall back
     * on a contiguous allcation.
     */
    if (rc == VERR_INTERNAL_ERROR || rc == VERR_NO_PHYS_MEMORY)
    {
        int rc2 = rtR0MemObjNativeAllocCont(ppMem, cb, false);
        if (RT_SUCCESS(rc2))
            rc = rc2;
    }

    return rc;

#else

    return rtR0MemObjNativeAllocCont(ppMem, cb, false);
#endif
}


int rtR0MemObjNativeEnterPhys(PPRTR0MEMOBJINTERNAL ppMem, RTHCPHYS Phys, size_t cb)
{
    /*
     * Validate the address range and create a descriptor for it.
     */
    int rc = VERR_ADDRESS_TOO_BIG;
    IOPhysicalAddress PhysAddr = Phys;
    if (PhysAddr == Phys)
    {
        IOMemoryDescriptor *pMemDesc = IOMemoryDescriptor::withPhysicalAddress(PhysAddr, cb, kIODirectionInOut);
        if (pMemDesc)
        {
            Assert(PhysAddr == pMemDesc->getPhysicalAddress());

            /*
             * Create the IPRT memory object.
             */
            PRTR0MEMOBJDARWIN pMemDarwin = (PRTR0MEMOBJDARWIN)rtR0MemObjNew(sizeof(*pMemDarwin), RTR0MEMOBJTYPE_PHYS, NULL, cb);
            if (pMemDarwin)
            {
                pMemDarwin->Core.u.Phys.PhysBase = PhysAddr;
                pMemDarwin->Core.u.Phys.fAllocated = false;
                pMemDarwin->pMemDesc = pMemDesc;
                *ppMem = &pMemDarwin->Core;
                return VINF_SUCCESS;
            }

            rc = VERR_NO_MEMORY;
            pMemDesc->release();
        }
    }
    else
        AssertMsgFailed(("%#llx\n", (unsigned long long)Phys));
    return rc;
}


int rtR0MemObjNativeLockUser(PPRTR0MEMOBJINTERNAL ppMem, void *pv, size_t cb)
{
    Assert(current_task() != kernel_task);
    int rc = VERR_MEMOBJ_INIT_FAILED;
    IOMemoryDescriptor *pMemDesc = IOMemoryDescriptor::withAddress((vm_address_t)pv, cb, kIODirectionInOut, current_task());
    if (pMemDesc)
    {
        IOReturn IORet = pMemDesc->prepare(kIODirectionInOut);
        if (IORet == kIOReturnSuccess)
        {
            /*
             * Create the IPRT memory object.
             */
            PRTR0MEMOBJDARWIN pMemDarwin = (PRTR0MEMOBJDARWIN)rtR0MemObjNew(sizeof(*pMemDarwin), RTR0MEMOBJTYPE_LOCK, pv, cb);
            if (pMemDarwin)
            {
                pMemDarwin->Core.u.Lock.Process = (RTPROCESS)current_task();
                pMemDarwin->pMemDesc = pMemDesc;
                *ppMem = &pMemDarwin->Core;
                return VINF_SUCCESS;
            }

            pMemDesc->complete();
            rc = VERR_NO_MEMORY;
        }
        else
            rc = VERR_LOCK_FAILED;
        pMemDesc->release();
    }
    return rc;
}


int rtR0MemObjNativeLockKernel(PPRTR0MEMOBJINTERNAL ppMem, void *pv, size_t cb)
{
    int rc = VERR_MEMOBJ_INIT_FAILED;
    IOMemoryDescriptor *pMemDesc = IOMemoryDescriptor::withAddress((vm_address_t)pv, cb, kIODirectionInOut, kernel_task);
    if (pMemDesc)
    {
        IOReturn IORet = pMemDesc->prepare(kIODirectionInOut);
        if (IORet == kIOReturnSuccess)
        {
            /*
             * Create the IPRT memory object.
             */
            PRTR0MEMOBJDARWIN pMemDarwin = (PRTR0MEMOBJDARWIN)rtR0MemObjNew(sizeof(*pMemDarwin), RTR0MEMOBJTYPE_LOCK, pv, cb);
            if (pMemDarwin)
            {
                pMemDarwin->Core.u.Lock.Process = NIL_RTPROCESS;
                pMemDarwin->pMemDesc = pMemDesc;
                *ppMem = &pMemDarwin->Core;
                return VINF_SUCCESS;
            }

            pMemDesc->complete();
            rc = VERR_NO_MEMORY;
        }
        else
            rc = VERR_LOCK_FAILED;
        pMemDesc->release();
    }
    return rc;
}


int rtR0MemObjNativeReserveKernel(PPRTR0MEMOBJINTERNAL ppMem, void *pvFixed, size_t cb, size_t uAlignment)
{
    return VERR_NOT_IMPLEMENTED;
}


int rtR0MemObjNativeReserveUser(PPRTR0MEMOBJINTERNAL ppMem, void *pvFixed, size_t cb, size_t uAlignment)
{
    return VERR_NOT_IMPLEMENTED;
}


int rtR0MemObjNativeMapKernel(PPRTR0MEMOBJINTERNAL ppMem, RTR0MEMOBJ pMemToMap, void *pvFixed, size_t uAlignment, unsigned fProt)
{
    /*
     * Must have a memory descriptor.
     */
    int rc = VERR_INVALID_PARAMETER;
    PRTR0MEMOBJDARWIN pMemToMapDarwin = (PRTR0MEMOBJDARWIN)pMemToMap;
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
                PRTR0MEMOBJDARWIN pMemDarwin = (PRTR0MEMOBJDARWIN)rtR0MemObjNew(sizeof(*pMemDarwin), RTR0MEMOBJTYPE_MAPPING,
                                                                                pv, pMemToMapDarwin->Core.cb);
                if (pMemDarwin)
                {
                    pMemDarwin->Core.u.Mapping.Process = NIL_RTPROCESS;
                    pMemDarwin->pMemMap = pMemMap;
                    *ppMem = &pMemDarwin->Core;
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


int rtR0MemObjNativeMapUser(PPRTR0MEMOBJINTERNAL ppMem, RTR0MEMOBJ pMemToMap, void *pvFixed, size_t uAlignment, unsigned fProt)
{
    /*
     * Must have a memory descriptor.
     */
    int rc = VERR_INVALID_PARAMETER;
    PRTR0MEMOBJDARWIN pMemToMapDarwin = (PRTR0MEMOBJDARWIN)pMemToMap;
    if (pMemToMapDarwin->pMemDesc)
    {
        Assert(current_task() != kernel_task);
        IOMemoryMap *pMemMap = pMemToMapDarwin->pMemDesc->map(current_task(), kIOMapAnywhere,
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
                PRTR0MEMOBJDARWIN pMemDarwin = (PRTR0MEMOBJDARWIN)rtR0MemObjNew(sizeof(*pMemDarwin), RTR0MEMOBJTYPE_MAPPING,
                                                                                pv, pMemToMapDarwin->Core.cb);
                if (pMemDarwin)
                {
                    pMemDarwin->Core.u.Mapping.Process = /*RTProcSelf()*/(RTPROCESS)current_task();
                    pMemDarwin->pMemMap = pMemMap;
                    *ppMem = &pMemDarwin->Core;
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
    PRTR0MEMOBJDARWIN pMemDarwin = (PRTR0MEMOBJDARWIN)pMem;

    /*
     * Get the memory descriptor.
     */
    IOMemoryDescriptor *pMemDesc = pMemDarwin->pMemDesc;
    if (!pMemDesc)
        pMemDesc = pMemDarwin->pMemMap->getMemoryDescriptor();
    AssertReturn(pMemDesc, NIL_RTHCPHYS);


    /*
     * If we've got a memory descriptor, use getPhysicalSegment64().
     */
    addr64_t Addr = pMemDesc->getPhysicalSegment64(iPage * PAGE_SIZE, NULL);
    AssertMsgReturn(Addr, ("iPage=%u\n", iPage), NIL_RTHCPHYS);
    RTHCPHYS PhysAddr = Addr;
    AssertMsgReturn(PhysAddr == Addr, ("PhysAddr=%VHp Addr=%RX64\n", PhysAddr, (uint64_t)Addr), NIL_RTHCPHYS);
    return PhysAddr;
}

