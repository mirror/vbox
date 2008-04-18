/* $Id$ */
/** @file
 * Incredibly Portable Runtime - Memory Allocation, Ring-0 Driver.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/string.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/param.h>
#include "r0drv/alloc-r0drv.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#ifdef RT_STRICT
# define RTR0MEM_STRICT
#endif

#ifdef RTR0MEM_STRICT
# define RTR0MEM_FENCE_EXTRA    16
#else
# define RTR0MEM_FENCE_EXTRA    0
#endif


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
#ifdef RTR0MEM_STRICT
/** Fence data. */
static uint8_t const g_abFence[RTR0MEM_FENCE_EXTRA] =
{
    0x77, 0x88, 0x66, 0x99,  0x55, 0xaa, 0x44, 0xbb,
    0x33, 0xcc, 0x22, 0xdd,  0x11, 0xee, 0x00, 0xff
};
#endif



/**
 * Allocates temporary memory.
 *
 * Temporary memory blocks are used for not too large memory blocks which
 * are believed not to stick around for too long. Using this API instead
 * of RTMemAlloc() not only gives the heap manager room for optimization
 * but makes the code easier to read.
 *
 * @returns Pointer to the allocated memory.
 * @returns NULL on failure.
 * @param   cb      Size in bytes of the memory block to allocated.
 */
RTDECL(void *)  RTMemTmpAlloc(size_t cb)
{
    return RTMemAlloc(cb);
}


/**
 * Allocates zero'ed temporary memory.
 *
 * Same as RTMemTmpAlloc() but the memory will be zero'ed.
 *
 * @returns Pointer to the allocated memory.
 * @returns NULL on failure.
 * @param   cb      Size in bytes of the memory block to allocated.
 */
RTDECL(void *)  RTMemTmpAllocZ(size_t cb)
{
    return RTMemAllocZ(cb);
}


/**
 * Free temporary memory.
 *
 * @param   pv      Pointer to memory block.
 */
RTDECL(void)    RTMemTmpFree(void *pv)
{
    return RTMemFree(pv);
}


/**
 * Allocates memory.
 *
 * @returns Pointer to the allocated memory.
 * @returns NULL on failure.
 * @param   cb      Size in bytes of the memory block to allocated.
 */
RTDECL(void *)  RTMemAlloc(size_t cb)
{
    PRTMEMHDR pHdr = rtMemAlloc(cb + RTR0MEM_FENCE_EXTRA, 0);
    if (pHdr)
    {
#ifdef RTR0MEM_STRICT
        pHdr->cbReq = cb;
        memcpy((uint8_t *)(pHdr + 1) + cb, &g_abFence[0], RTR0MEM_FENCE_EXTRA);
#endif
        return pHdr + 1;
    }
    return NULL;
}


/**
 * Allocates zero'ed memory.
 *
 * Instead of memset(pv, 0, sizeof()) use this when you want zero'ed
 * memory. This keeps the code smaller and the heap can skip the memset
 * in about 0.42% of calls :-).
 *
 * @returns Pointer to the allocated memory.
 * @returns NULL on failure.
 * @param   cb      Size in bytes of the memory block to allocated.
 */
RTDECL(void *)  RTMemAllocZ(size_t cb)
{
    PRTMEMHDR pHdr = rtMemAlloc(cb + RTR0MEM_FENCE_EXTRA, RTMEMHDR_FLAG_ZEROED);
    if (pHdr)
    {
#ifdef RTR0MEM_STRICT
        pHdr->cbReq = cb;
        memcpy((uint8_t *)(pHdr + 1) + cb, &g_abFence[0], RTR0MEM_FENCE_EXTRA);
        return memset(pHdr + 1, 0, cb);
#else
        return memset(pHdr + 1, 0, pHdr->cb);
#endif
    }
    return NULL;
}


/**
 * Reallocates memory.
 *
 * @returns Pointer to the allocated memory.
 * @returns NULL on failure.
 * @param   pvOld   The memory block to reallocate.
 * @param   cbNew   The new block size (in bytes).
 */
RTDECL(void *) RTMemRealloc(void *pvOld, size_t cbNew)
{
    if (!cbNew)
        RTMemFree(pvOld);
    else if (!pvOld)
        return RTMemAlloc(cbNew);
    else
    {
        PRTMEMHDR pHdrOld = (PRTMEMHDR)pvOld - 1;
        if (pHdrOld->u32Magic == RTMEMHDR_MAGIC)
        {
            PRTMEMHDR pHdrNew;
            if (pHdrOld->cb >= cbNew && pHdrOld->cb - cbNew <= 128)
                return pvOld;
            pHdrNew = rtMemAlloc(cbNew + RTR0MEM_FENCE_EXTRA, 0);
            if (pHdrNew)
            {
                size_t cbCopy = RT_MIN(pHdrOld->cb, pHdrNew->cb);
                memcpy(pHdrNew + 1, pvOld, cbCopy);
#ifdef RTR0MEM_STRICT
                pHdrNew->cbReq = cbNew;
                memcpy((uint8_t *)(pHdrNew + 1) + cbNew, &g_abFence[0], RTR0MEM_FENCE_EXTRA);
                AssertReleaseMsg(!memcmp((uint8_t *)(pHdrOld + 1) + pHdrOld->cbReq, &g_abFence[0], RTR0MEM_FENCE_EXTRA),
                                 ("pHdr=%p pvOld=%p cb=%zu cbNew=%zu\n"
                                  "fence:    %.*Rhxs\n"
                                  "expected: %.*Rhxs\n",
                                  pHdrOld, pvOld, pHdrOld->cb, cbNew,
                                  RTR0MEM_FENCE_EXTRA, (uint8_t *)(pHdrOld + 1) + pHdrOld->cb,
                                  RTR0MEM_FENCE_EXTRA, &g_abFence[0]));
#endif
                rtMemFree(pHdrOld);
                return pHdrNew + 1;
            }
        }
        else
            AssertMsgFailed(("pHdrOld->u32Magic=%RX32 pvOld=%p cbNew=%#zx\n", pHdrOld->u32Magic, pvOld, cbNew));
    }

    return NULL;
}


/**
 * Free memory related to an virtual machine
 *
 * @param   pv      Pointer to memory block.
 */
RTDECL(void) RTMemFree(void *pv)
{
    PRTMEMHDR pHdr;
    if (!pv)
        return;
    pHdr = (PRTMEMHDR)pv - 1;
    if (pHdr->u32Magic == RTMEMHDR_MAGIC)
    {
        Assert(!(pHdr->fFlags & RTMEMHDR_FLAG_EXEC));
#ifdef RTR0MEM_STRICT
        AssertReleaseMsg(!memcmp((uint8_t *)(pHdr + 1) + pHdr->cbReq, &g_abFence[0], RTR0MEM_FENCE_EXTRA),
                         ("pHdr=%p pv=%p cb=%zu\n"
                          "fence:    %.*Rhxs\n"
                          "expected: %.*Rhxs\n",
                          pHdr, pv, pHdr->cb, pv,
                          RTR0MEM_FENCE_EXTRA, (uint8_t *)(pHdr + 1) + pHdr->cb,
                          RTR0MEM_FENCE_EXTRA, &g_abFence[0]));
#endif
        rtMemFree(pHdr);
    }
    else
        AssertMsgFailed(("pHdr->u32Magic=%RX32 pv=%p\n", pHdr->u32Magic, pv));
}


/**
 * Allocates memory which may contain code.
 *
 * @returns Pointer to the allocated memory.
 * @returns NULL on failure.
 * @param   cb      Size in bytes of the memory block to allocate.
 */
RTDECL(void *)    RTMemExecAlloc(size_t cb)
{
    PRTMEMHDR pHdr = rtMemAlloc(cb + RTR0MEM_FENCE_EXTRA, RTMEMHDR_FLAG_EXEC);
    if (pHdr)
    {
#ifdef RTR0MEM_STRICT
        pHdr->cbReq = cb;
        memcpy((uint8_t *)(pHdr + 1) + cb, &g_abFence[0], RTR0MEM_FENCE_EXTRA);
#endif
        return pHdr + 1;
    }
    return NULL;
}


/**
 * Free executable/read/write memory allocated by RTMemExecAlloc().
 *
 * @param   pv      Pointer to memory block.
 */
RTDECL(void)      RTMemExecFree(void *pv)
{
    PRTMEMHDR pHdr;
    if (!pv)
        return;
    pHdr = (PRTMEMHDR)pv - 1;
    if (pHdr->u32Magic == RTMEMHDR_MAGIC)
    {
#ifdef RTR0MEM_STRICT
        AssertReleaseMsg(!memcmp((uint8_t *)(pHdr + 1) + pHdr->cbReq, &g_abFence[0], RTR0MEM_FENCE_EXTRA),
                         ("pHdr=%p pv=%p cb=%zu\n"
                          "fence:    %.*Rhxs\n"
                          "expected: %.*Rhxs\n",
                          pHdr, pv, pHdr->cb,
                          RTR0MEM_FENCE_EXTRA, (uint8_t *)(pHdr + 1) + pHdr->cb,
                          RTR0MEM_FENCE_EXTRA, &g_abFence[0]));
#endif
        rtMemFree(pHdr);
    }
    else
        AssertMsgFailed(("pHdr->u32Magic=%RX32 pv=%p\n", pHdr->u32Magic, pv));
}

