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
#include <malloc.h>

#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/types.h>
#include "r0drv/alloc-r0drv.h"


/**
 * OS specific allocation function.
 */
PRTMEMHDR rtMemAlloc(size_t cb, uint32_t fFlags)
{
    Assert(cb != sizeof(void *));
    PRTMEMHDR pHdr;
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
    /** @todo: implement RTMemContAlloc in Solaris */
    /* ddi_umem_alloc without PAGEABLE flag might produce contiguous physical memory, but
        the documentation doesn't talk about contiguous at all :( 
       If we can use ddi_umem_alloc we need to keep track of the ddi_umem_cookie
       which kernel allocates, but the ContFree() function only passes us back
       the address. Maybe we could for each ContAlloc build a linked list of
       structures that have the cookie and corresponding virtual address. */
    return NULL;
}

RTR0DECL(void) RTMemContFree(void *pv, size_t cb)
{
    /** @todo implement RTMemContFree on solaris, see RTMemContAlloc() for details. */
    NOREF(pv);
    NOREF(cb);
}
