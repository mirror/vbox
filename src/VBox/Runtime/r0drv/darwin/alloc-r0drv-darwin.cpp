/* $Id$ */
/** @file
 * innotek Portable Runtime - Memory Allocation, Ring-0 Driver, Darwin.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
#include "the-darwin-kernel.h"

#include <iprt/alloc.h>
#include <iprt/assert.h>
#include "r0drv/alloc-r0drv.h"


/**
 * OS specific allocation function.
 */
PRTMEMHDR rtMemAlloc(size_t cb, uint32_t fFlags)
{
    Assert(cb != sizeof(void *)); /* 99% of pointer sized allocations are wrong. */
    PRTMEMHDR pHdr = (PRTMEMHDR)IOMalloc(cb + sizeof(*pHdr));
    if (pHdr)
    {
        pHdr->u32Magic  = RTMEMHDR_MAGIC;
        pHdr->fFlags    = fFlags;
        pHdr->cb        = cb;
        pHdr->u32Padding= 0;
    }
    else
        printf("rmMemAlloc(%#x, %#x) failed\n", cb + sizeof(*pHdr), fFlags);
    return pHdr;
}


/**
 * OS specific free function.
 */
void rtMemFree(PRTMEMHDR pHdr)
{
    pHdr->u32Magic += 1;
    IOFree(pHdr, pHdr->cb + sizeof(*pHdr));
}


RTR0DECL(void *) RTMemContAlloc(PRTCCPHYS pPhys, size_t cb)
{
    /*
     * validate input.
     */
    AssertPtr(pPhys);
    Assert(cb > 0);

    /*
     * Allocate the memory and ensure that the API is still providing
     * memory that's always below 4GB.
     */
    cb = RT_ALIGN_Z(cb, PAGE_SIZE);
    IOPhysicalAddress PhysAddr;
    void *pv = IOMallocContiguous(cb, PAGE_SIZE, &PhysAddr);
    if (pv)
    {
        if (PhysAddr + (cb - 1) <= (IOPhysicalAddress)0xffffffff)
        {
            if (!((uintptr_t)pv & PAGE_OFFSET_MASK))
            {
                *pPhys = PhysAddr;
                return pv;
            }
            AssertMsgFailed(("IOMallocContiguous didn't return a page aligned address - %p!\n", pv));
        }
        else
            AssertMsgFailed(("IOMallocContiguous returned high address! PhysAddr=%RX64 cb=%#zx\n", (uint64_t)PhysAddr, cb));
        IOFreeContiguous(pv, cb);
    }
    return NULL;
}


RTR0DECL(void) RTMemContFree(void *pv, size_t cb)
{
    if (pv)
    {
        Assert(cb > 0);
        AssertMsg(!((uintptr_t)pv & PAGE_OFFSET_MASK), ("pv=%p\n", pv));

        cb = RT_ALIGN_Z(cb, PAGE_SIZE);
        IOFreeContiguous(pv, cb);
    }
}

