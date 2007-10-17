/* $Id$ */
/** @file
 * innotek Portable Runtime - Memory Allocation, Ring-0 Driver, Solaris.
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
#include "the-solaris-kernel.h"

#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/types.h>
#include <iprt/param.h>
#include "r0drv/alloc-r0drv.h"


/**
 * OS specific allocation function.
 */
PRTMEMHDR rtMemAlloc(size_t cb, uint32_t fFlags)
{
    Assert(cb != sizeof(void *));
    PRTMEMHDR pHdr;
#ifdef RT_ARCH_AMD64
    if (fFlags & RTMEMHDR_FLAG_EXEC)
        pHdr = (PRTMEMHDR)segkmem_alloc(heaptext_arena, RT_ALIGN_Z(cb + sizeof(*pHdr), PAGE_SIZE), KM_SLEEP);
    else
#endif
    if (fFlags & RTMEMHDR_FLAG_ZEROED)
        pHdr = (PRTMEMHDR)kmem_zalloc(cb + sizeof(*pHdr), KM_SLEEP);
    else
        pHdr = (PRTMEMHDR)kmem_alloc(cb + sizeof(*pHdr), KM_SLEEP);
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
#ifdef RT_ARCH_AMD64
    if (pHdr->fFlags & RTMEMHDR_FLAG_EXEC)
        segkmem_free(heaptext_arena, pHdr, RT_ALIGN_Z(pHdr->cb + sizeof(*pHdr), PAGE_SIZE));
    else
#endif
        kmem_free(pHdr, pHdr->cb + sizeof(*pHdr));
}


/**
 * Allocates physical contiguous memory (below 4GB).
 * The allocation is page aligned and the content is undefined.
 *
 * @returns Pointer to the memory block. This is page aligned.
 * @param   pPhys   Where to store the physical address.
 * @param   cb      The allocation size in bytes. This is always
 *                  rounded up to PAGE_SIZE.
 */
RTR0DECL(void *) RTMemContAlloc(PRTCCPHYS pPhys, size_t cb)
{
    AssertPtr(pPhys);
    Assert(cb > 0);

    /* Allocate physically contiguous page-aligned memory. */
    caddr_t virtAddr;
    int rc = i_ddi_mem_alloc(NULL, &g_SolarisX86PhysMemLimits, cb, 1, 0, NULL, &virtAddr, NULL, NULL);
    if (rc != DDI_SUCCESS)
        return NULL;

    *pPhys = PAGE_SIZE * hat_getpfnum(kas.a_hat, virtAddr);
    return virtAddr;
}


RTR0DECL(void) RTMemContFree(void *pv, size_t cb)
{
    NOREF(cb);
    if (pv)
        i_ddi_mem_free(pv, NULL);
}

