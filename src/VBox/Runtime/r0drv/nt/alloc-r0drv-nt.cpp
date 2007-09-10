/* $Id$ */
/** @file
 * innotek Portable Runtime - Memory Allocation, Ring-0 Driver, NT.
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

#include <iprt/alloc.h>
#include <iprt/assert.h>
#include "r0drv/alloc-r0drv.h"


/**
 * OS specific allocation function.
 */
PRTMEMHDR rtMemAlloc(size_t cb, uint32_t fFlags)
{
    Assert(cb != sizeof(void *)); /* 99% of pointer sized allocations are wrong. */
    PRTMEMHDR pHdr = (PRTMEMHDR)ExAllocatePoolWithTag(NonPagedPool, cb + sizeof(*pHdr), IPRT_NT_POOL_TAG);
    if (pHdr)
    {
        pHdr->u32Magic  = RTMEMHDR_MAGIC;
        pHdr->fFlags    = fFlags;
        pHdr->cb        = cb;
        pHdr->u32Padding= 0;
    }
    return pHdr;
}


/**
 * OS specific free function.
 */
void rtMemFree(PRTMEMHDR pHdr)
{
    pHdr->u32Magic += 1;
    ExFreePool(pHdr);
}


/**
 * Allocates physical contiguous memory (below 4GB).
 * The allocation is page aligned and its contents is undefined.
 *
 * @returns Pointer to the memory block. This is page aligned.
 * @param   pPhys   Where to store the physical address.
 * @param   cb      The allocation size in bytes. This is always
 *                  rounded up to PAGE_SIZE.
 */
RTR0DECL(void *) RTMemContAlloc(PRTCCPHYS pPhys, size_t cb)
{
    /*
     * validate input.
     */
    Assert(VALID_PTR(pPhys));
    Assert(cb > 0);

    /*
     * Allocate and get physical address.
     * Make sure the return is page aligned.
     */
    PHYSICAL_ADDRESS MaxPhysAddr;
    MaxPhysAddr.HighPart = 0;
    MaxPhysAddr.LowPart = 0xffffffff;
    cb = RT_ALIGN_Z(cb, PAGE_SIZE);
    void *pv = MmAllocateContiguousMemory(cb, MaxPhysAddr);
    if (pv)
    {
        if (!((uintptr_t)pv & PAGE_OFFSET_MASK))    /* paranoia */
        {
            PHYSICAL_ADDRESS PhysAddr = MmGetPhysicalAddress(pv);
            if (!PhysAddr.HighPart)                 /* paranoia */
            {
                *pPhys = (RTCCPHYS)PhysAddr.LowPart;
                return pv;
            }

            /* failure */
            AssertMsgFailed(("MMAllocContiguousMemory returned high address! PhysAddr=%RX64\n", (uint64_t)PhysAddr.QuadPart));
        }
        else
            AssertMsgFailed(("MMAllocContiguousMemory didn't return a page aligned address - %p!\n", pv));

        MmFreeContiguousMemory(pv);
    }

    return NULL;
}


/**
 * Frees memory allocated ysing RTMemContAlloc().
 *
 * @param   pv      Pointer to return from RTMemContAlloc().
 * @param   cb      The cb parameter passed to RTMemContAlloc().
 */
RTR0DECL(void) RTMemContFree(void *pv, size_t cb)
{
    if (pv)
    {
        Assert(cb > 0); NOREF(cb);
        AssertMsg(!((uintptr_t)pv & PAGE_OFFSET_MASK), ("pv=%p\n", pv));
        MmFreeContiguousMemory(pv);
    }
}

