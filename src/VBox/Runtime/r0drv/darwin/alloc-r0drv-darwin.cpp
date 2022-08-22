/* $Id$ */
/** @file
 * IPRT - Memory Allocation, Ring-0 Driver, Darwin.
 */

/*
 * Copyright (C) 2006-2022 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#pragma GCC diagnostic ignored "-Wdeprecated-declarations" /* (IOMallocContiguous et al are deprecated) */
#include "the-darwin-kernel.h"
#include "internal/iprt.h"
#include <iprt/mem.h>
#include <iprt/memobj.h>

#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/thread.h>
#include "r0drv/alloc-r0drv.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
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
    IPRT_DARWIN_SAVE_EFL_AC();

    if (RT_UNLIKELY(fFlags & RTMEMHDR_FLAG_ANY_CTX))
        return VERR_NOT_SUPPORTED;

    PRTMEMHDR pHdr;
    if (fFlags & RTMEMHDR_FLAG_EXEC)
    {
        RTR0MEMOBJ hMemObj;
        int rc = RTR0MemObjAllocPage(&hMemObj, cb + sizeof(RTMEMDARWINHDREX), true /*fExecutable*/);
        if (RT_FAILURE(rc))
        {
            IPRT_DARWIN_RESTORE_EFL_AC();
            return rc;
        }
        PRTMEMDARWINHDREX pExHdr = (PRTMEMDARWINHDREX)RTR0MemObjAddress(hMemObj);
        pExHdr->hMemObj = hMemObj;
        pHdr = &pExHdr->Hdr;
#if 1 /*fExecutable isn't currently honored above. */
        rc = RTR0MemObjProtect(hMemObj, 0, RTR0MemObjSize(hMemObj), RTMEM_PROT_READ | RTMEM_PROT_WRITE | RTMEM_PROT_EXEC);
        AssertRC(rc);
#endif
    }
    else
    {
        pHdr = (PRTMEMHDR)IOMalloc(cb + sizeof(*pHdr));
        if (RT_UNLIKELY(!pHdr))
        {
            printf("rtR0MemAllocEx(%#zx, %#x) failed\n", cb + sizeof(*pHdr), fFlags);
            IPRT_DARWIN_RESTORE_EFL_AC();
            return VERR_NO_MEMORY;
        }
    }

    pHdr->u32Magic  = RTMEMHDR_MAGIC;
    pHdr->fFlags    = fFlags;
    pHdr->cb        = cb;
    pHdr->cbReq     = cb;
    *ppHdr = pHdr;

    IPRT_DARWIN_RESTORE_EFL_AC();
    return VINF_SUCCESS;
}


/**
 * OS specific free function.
 */
DECLHIDDEN(void) rtR0MemFree(PRTMEMHDR pHdr)
{
    IPRT_DARWIN_SAVE_EFL_AC();

    pHdr->u32Magic += 1;
    if (pHdr->fFlags & RTMEMHDR_FLAG_EXEC)
    {
        PRTMEMDARWINHDREX pExHdr = RT_FROM_MEMBER(pHdr, RTMEMDARWINHDREX, Hdr);
        int rc = RTR0MemObjFree(pExHdr->hMemObj, false /*fFreeMappings*/);
        AssertRC(rc);
    }
    else
        IOFree(pHdr, pHdr->cb + sizeof(*pHdr));

    IPRT_DARWIN_RESTORE_EFL_AC();
}


RTR0DECL(void *) RTMemContAlloc(PRTCCPHYS pPhys, size_t cb)
{
    /*
     * validate input.
     */
    AssertPtr(pPhys);
    Assert(cb > 0);
    RT_ASSERT_PREEMPTIBLE();
    IPRT_DARWIN_SAVE_EFL_AC();

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
                IPRT_DARWIN_RESTORE_EFL_AC();
                return pv;
            }
            AssertMsgFailed(("IOMallocContiguous didn't return a page aligned address - %p!\n", pv));
        }
        else
            AssertMsgFailed(("IOMallocContiguous returned high address! PhysAddr=%RX64 cb=%#zx\n", (uint64_t)PhysAddr, cb));
        IOFreeContiguous(pv, cb);
    }

    IPRT_DARWIN_RESTORE_EFL_AC();
    return NULL;
}


RTR0DECL(void) RTMemContFree(void *pv, size_t cb)
{
    RT_ASSERT_PREEMPTIBLE();
    if (pv)
    {
        Assert(cb > 0);
        AssertMsg(!((uintptr_t)pv & PAGE_OFFSET_MASK), ("pv=%p\n", pv));
        IPRT_DARWIN_SAVE_EFL_AC();

        cb = RT_ALIGN_Z(cb, PAGE_SIZE);
        IOFreeContiguous(pv, cb);

        IPRT_DARWIN_RESTORE_EFL_AC();
    }
}

