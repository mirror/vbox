/* $Id$ */
/** @file
 * IPRT - Memory Allocation, Ring-0 Driver, Darwin.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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
#include "internal/iprt.h"
#include <iprt/mem.h>
#include <iprt/memobj.h>

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/thread.h>
#include "r0drv/alloc-r0drv.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Extended header used for headers marked with RTMEMHDR_FLAG_EXEC.
 *
 * This is used with allocating executable memory, for things like generated
 * code and loaded modules.
 */
typedef struct RTMEMDARWINHDREX
{
    /** The associated memory object. */
    RTR0MEMOBJ          hMemObj;
    /** Alignment padding. */
    uint8_t             abPadding[ARCH_BITS == 32 ? 12 : 8];
    /** The header we present to the generic API. */
    RTMEMHDR            Hdr;
} RTMEMDARWINHDREX;
AssertCompileSize(RTMEMDARWINHDREX, 32);
/** Pointer to an extended memory header. */
typedef RTMEMDARWINHDREX *PRTMEMDARWINHDREX;


/**
 * OS specific allocation function.
 */
DECLHIDDEN(int) rtR0MemAllocEx(size_t cb, uint32_t fFlags, PRTMEMHDR *ppHdr)
{
    if (RT_UNLIKELY(fFlags & RTMEMHDR_FLAG_ANY_CTX))
        return VERR_NOT_SUPPORTED;

    PRTMEMHDR pHdr;
    if (fFlags & RTMEMHDR_FLAG_EXEC)
    {
        RTR0MEMOBJ hMemObj;
        int rc = RTR0MemObjAllocPage(&hMemObj, cb + sizeof(RTMEMDARWINHDREX), true /*fExecutable*/);
        if (RT_FAILURE(rc))
            return rc;
        PRTMEMDARWINHDREX pExHdr = (PRTMEMDARWINHDREX)RTR0MemObjAddress(hMemObj);
        pExHdr->hMemObj = hMemObj;
        pHdr = &pExHdr->Hdr;
    }
    else
    {

        pHdr = (PRTMEMHDR)IOMalloc(cb + sizeof(*pHdr));
        if (RT_UNLIKELY(!pHdr))
        {
            printf("rtR0MemAllocEx(%#zx, %#x) failed\n", cb + sizeof(*pHdr), fFlags);
            return VERR_NO_MEMORY;
        }
    }

    pHdr->u32Magic  = RTMEMHDR_MAGIC;
    pHdr->fFlags    = fFlags;
    pHdr->cb        = cb;
    pHdr->cbReq     = cb;
    *ppHdr = pHdr;;
    return VINF_SUCCESS;
}


/**
 * OS specific free function.
 */
DECLHIDDEN(void) rtR0MemFree(PRTMEMHDR pHdr)
{
    pHdr->u32Magic += 1;
    if (pHdr->fFlags & RTMEMHDR_FLAG_EXEC)
    {
        PRTMEMDARWINHDREX pExHdr = RT_FROM_MEMBER(pHdr, RTMEMDARWINHDREX, Hdr);
        int rc = RTR0MemObjFree(pExHdr->hMemObj, false /*fFreeMappings*/);
        AssertRC(rc);
    }
    else
        IOFree(pHdr, pHdr->cb + sizeof(*pHdr));
}


RTR0DECL(void *) RTMemContAlloc(PRTCCPHYS pPhys, size_t cb)
{
    /*
     * validate input.
     */
    AssertPtr(pPhys);
    Assert(cb > 0);
    RT_ASSERT_PREEMPTIBLE();

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
    RT_ASSERT_PREEMPTIBLE();
    if (pv)
    {
        Assert(cb > 0);
        AssertMsg(!((uintptr_t)pv & PAGE_OFFSET_MASK), ("pv=%p\n", pv));

        cb = RT_ALIGN_Z(cb, PAGE_SIZE);
        IOFreeContiguous(pv, cb);
    }
}

