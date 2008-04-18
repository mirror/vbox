/* $Id$ */
/** @file
 * Incredibly Portable Runtime - Memory Allocation, Ring-0 Driver, FreeBSD.
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

#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/param.h>

#include "r0drv/alloc-r0drv.h"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/* These two statements will define two globals and add initializers
   and destructors that will be called at load/unload time (I think). */
MALLOC_DEFINE(M_IPRTHEAP, "iprtheap", "Incredibly Portable Runtime - heap");
MALLOC_DEFINE(M_IPRTCONT, "iprtcont", "Incredibly Portable Runtime - contiguous");


PRTMEMHDR rtMemAlloc(size_t cb, uint32_t fFlags)
{
    PRTMEMHDR pHdr;

    /** @todo Just like OS/2, FreeBSD doesn't need this header. */
    pHdr = (PRTMEMHDR)malloc(cb + sizeof(RTMEMHDR), M_IPRTHEAP,
                             fFlags & RTMEMHDR_FLAG_ZEROED ? M_NOWAIT | M_ZERO : M_NOWAIT);
    if (pHdr)
    {
        pHdr->u32Magic   = RTMEMHDR_MAGIC;
        pHdr->fFlags     = fFlags;
        pHdr->cb         = cb;
        pHdr->cbReq      = cb;
        return pHdr;
    }
    return NULL;
}


void rtMemFree(PRTMEMHDR pHdr)
{
    pHdr->u32Magic += 1;
    free(pHdr, M_IPRTHEAP);
}


RTR0DECL(void *) RTMemContAlloc(PRTCCPHYS pPhys, size_t cb)
{
    void *pv;

    /*
     * Validate input.
     */
    AssertPtr(pPhys);
    Assert(cb > 0);

    /*
     * This API works in pages, so no need to do any size aligning.
     */
    pv = contigmalloc(cb,                   /* size */
                      M_IPRTCONT,           /* type */
                      M_NOWAIT | M_ZERO,    /* flags */
                      0,                    /* lowest physical address*/
                      _4G-1,                /* highest physical address */
                      PAGE_SIZE,            /* alignment. */
                      0);                   /* boundrary */
    if (pv)
    {
        Assert(!((uintptr_t)pv & PAGE_OFFSET_MASK));
        *pPhys = vtophys(pv);
        Assert(!(*pPhys & PAGE_OFFSET_MASK));
    }
    return pv;
}


RTR0DECL(void) RTMemContFree(void *pv, size_t cb)
{
    if (pv)
    {
        AssertMsg(!((uintptr_t)pv & PAGE_OFFSET_MASK), ("pv=%p\n", pv));
        contigfree(pv, cb, M_IPRTCONT);
    }
}

