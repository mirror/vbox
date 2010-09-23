/* $Id$ */
/** @file
 * IPRT - Memory Allocation, Ring-0 Driver, Solaris.
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
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
#include "../the-solaris-kernel.h"
#include "internal/iprt.h"
#include <iprt/mem.h>

#include <iprt/assert.h>
#include <iprt/log.h>
#include <iprt/param.h>
#include <iprt/thread.h>
#include "r0drv/alloc-r0drv.h"



/**
 * OS specific allocation function.
 */
int rtR0MemAllocEx(size_t cb, uint32_t fFlags, PRTMEMHDR *ppHdr)
{
    size_t      cbAllocated = cb;
    PRTMEMHDR   pHdr;

#ifdef RT_ARCH_AMD64
    if (fFlags & RTMEMHDR_FLAG_EXEC)
    {
        AssertReturn(!(fFlags & RTMEMHDR_FLAG_ANY_CTX), NULL);
        cbAllocated = RT_ALIGN_Z(cb + sizeof(*pHdr), PAGE_SIZE) - sizeof(*pHdr);
        pHdr = (PRTMEMHDR)vbi_text_alloc(cbAllocated + sizeof(*pHdr));
    }
    else
#endif
    {
        unsigned fKmFlags = fFlags & RTMEMHDR_FLAG_ANY_CTX_ALLOC ? KM_NOSLEEP : KM_SLEEP;
        if (fFlags & RTMEMHDR_FLAG_ZEROED)
            pHdr = (PRTMEMHDR)kmem_zalloc(cb + sizeof(*pHdr), fKmFlags);
        else
            pHdr = (PRTMEMHDR)kmem_alloc(cb + sizeof(*pHdr), fKmFlags);
    }
    if (RT_UNLIKELY(!pHdr))
    {
        LogRel(("rtMemAllocEx(%u, %#x) failed\n", (unsigned)cb + sizeof(*pHdr), fFlags));
        return VERR_NO_MEMORY;
    }

    pHdr->u32Magic  = RTMEMHDR_MAGIC;
    pHdr->fFlags    = fFlags;
    pHdr->cb        = cbAllocated;
    pHdr->cbReq     = cb;

    *ppHdr = pHdr;
    return VINF_SUCCESS;
}


/**
 * OS specific free function.
 */
void rtR0MemFree(PRTMEMHDR pHdr)
{
    pHdr->u32Magic += 1;
#ifdef RT_ARCH_AMD64
    if (pHdr->fFlags & RTMEMHDR_FLAG_EXEC)
        vbi_text_free(pHdr, pHdr->cb + sizeof(*pHdr));
    else
#endif
        kmem_free(pHdr, pHdr->cb + sizeof(*pHdr));
}


RTR0DECL(void *) RTMemContAlloc(PRTCCPHYS pPhys, size_t cb)
{
    AssertPtrReturn(pPhys, NULL);
    AssertReturn(cb > 0, NULL);
    RT_ASSERT_PREEMPTIBLE();

    /* Allocate physically contiguous (< 4GB) page-aligned memory. */
    uint64_t physAddr = _4G -1;
    caddr_t virtAddr  = vbi_contig_alloc(&physAddr, cb);
    if (virtAddr == NULL)
    {
        LogRel(("vbi_contig_alloc failed to allocate %u bytes\n", cb));
        return NULL;
    }

    Assert(physAddr < _4G);
    *pPhys = physAddr;
    return virtAddr;
}


RTR0DECL(void) RTMemContFree(void *pv, size_t cb)
{
    RT_ASSERT_PREEMPTIBLE();
    if (pv)
        vbi_contig_free(pv, cb);
}

