/* $Id $ */
/** @file
 * Incredibly Portable Runtime Testcase - Simple Heap.
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
#include <iprt/heap.h>
#include <iprt/initterm.h>
#include <iprt/err.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/param.h>
#include <iprt/assert.h>


int main(int argc, char *argv[])
{
    /*
     * Init runtime.
     */
    int rc = RTR3Init();
    if (RT_FAILURE(rc))
    {
        RTPrintf("RTR3Init failed: %Vrc\n", rc);
        return 1;
    }
    RTPrintf("tstHeapSimple: TESTING...\n");

    /*
     * Create a heap.
     */
    static uint8_t s_abMem[128*1024];
    RTHEAPSIMPLE Heap;
    rc = RTHeapSimpleInit(&Heap, &s_abMem[1], sizeof(s_abMem) - 1);
    if (RT_FAILURE(rc))
    {
        RTPrintf("RTHeapSimpleInit failed: %Vrc\n", rc);
        return 1;
    }

    /*
     * Try allocate.
     */
    static struct
    {
        size_t      cb;
        unsigned    uAlignment;
        void       *pvAlloc;
        unsigned    iFreeOrder;
    } aOps[] =
    {
        {        16,          0,    NULL,  0 },  // 0
        {        16,          4,    NULL,  1 },
        {        16,          8,    NULL,  2 },
        {        16,         16,    NULL,  5 },
        {        16,         32,    NULL,  4 },
        {        32,          0,    NULL,  3 },  // 5
        {        31,          0,    NULL,  6 },
        {      1024,          0,    NULL,  8 },
        {      1024,         32,    NULL, 10 },
        {      1024,         32,    NULL, 12 },
        { PAGE_SIZE,  PAGE_SIZE,    NULL, 13 },  // 10
        {      1024,         32,    NULL,  9 },
        { PAGE_SIZE,         32,    NULL, 11 },
        { PAGE_SIZE,  PAGE_SIZE,    NULL, 14 },
        {        16,          0,    NULL, 15 },
        {        9,           0,    NULL,  7 },  // 15
        {        16,          0,    NULL,  7 },
        {        36,          0,    NULL,  7 },
        {        16,          0,    NULL,  7 },
        {     12344,          0,    NULL,  7 },
        {        50,          0,    NULL,  7 },  // 20
        {        16,          0,    NULL,  7 },
    };
    unsigned i;
    RTHeapSimpleDump(Heap, (PFNRTHEAPSIMPLEPRINTF)RTPrintf);
    size_t cbBefore = RTHeapSimpleGetFreeSize(Heap);
    static char szFill[] = "01234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

    /* allocate */
    for (i = 0; i < ELEMENTS(aOps); i++)
    {
        aOps[i].pvAlloc = RTHeapSimpleAlloc(Heap, aOps[i].cb, aOps[i].uAlignment);
        if (!aOps[i].pvAlloc)
        {
            RTPrintf("Failure: RTHeapSimpleAlloc(%p, %#x, %#x,) -> NULL i=%d\n", (void *)Heap, aOps[i].cb, aOps[i].uAlignment, i);
            return 1;
        }
        memset(aOps[i].pvAlloc, szFill[i], aOps[i].cb);
        if (RT_ALIGN_P(aOps[i].pvAlloc, (aOps[i].uAlignment ? aOps[i].uAlignment : 8)) != aOps[i].pvAlloc)
        {
            RTPrintf("Failure: RTHeapSimpleAlloc(%p, %#x, %#x,) -> %p\n", (void *)Heap, aOps[i].cb, aOps[i].uAlignment, i);
            return 1;
        }
    }

    /* free and allocate the same node again. */
    for (i = 0; i < ELEMENTS(aOps); i++)
    {
        if (!aOps[i].pvAlloc)
            continue;
        //RTPrintf("debug: i=%d pv=%#x cb=%#zx align=%#zx cbReal=%#zx\n", i, aOps[i].pvAlloc,
        //         aOps[i].cb, aOps[i].uAlignment, RTHeapSimpleSize(Heap, aOps[i].pvAlloc));
        size_t cbBeforeSub = RTHeapSimpleGetFreeSize(Heap);
        RTHeapSimpleFree(Heap, aOps[i].pvAlloc);
        size_t cbAfterSubFree = RTHeapSimpleGetFreeSize(Heap);

        void *pv;
        pv = RTHeapSimpleAlloc(Heap, aOps[i].cb, aOps[i].uAlignment);
        if (!pv)
        {
            RTPrintf("Failure: RTHeapSimpleAlloc(%p, %#x, %#x,) -> NULL i=%d\n", (void *)Heap, aOps[i].cb, aOps[i].uAlignment, i);
            return 1;
        }
        //RTPrintf("debug: i=%d pv=%p cbReal=%#zx cbBeforeSub=%#zx cbAfterSubFree=%#zx cbAfterSubAlloc=%#zx \n", i, pv, RTHeapSimpleSize(Heap, pv),
        //         cbBeforeSub, cbAfterSubFree, RTHeapSimpleGetFreeSize(Heap));
        if (pv != aOps[i].pvAlloc)
            RTPrintf("Warning: Free+Alloc returned different address. new=%p old=%p i=%d\n", pv, aOps[i].pvAlloc, i);
        aOps[i].pvAlloc = pv;
        size_t cbAfterSubAlloc = RTHeapSimpleGetFreeSize(Heap);
        if (cbBeforeSub != cbAfterSubAlloc)
        {
            RTPrintf("Warning: cbBeforeSub=%#zx cbAfterSubFree=%#zx cbAfterSubAlloc=%#zx. i=%d\n",
                     cbBeforeSub, cbAfterSubFree, cbAfterSubAlloc, i);
            //return 1; - won't work correctly until we start creating free block instead of donating memory on alignment.
        }
    }

    /* free it in a specific order. */
    int cFreed = 0;
    for (i = 0; i < ELEMENTS(aOps); i++)
    {
        unsigned j;
        for (j = 0; j < ELEMENTS(aOps); j++)
        {
            if (    aOps[j].iFreeOrder != i
                ||  !aOps[j].pvAlloc)
                continue;
            //RTPrintf("j=%d i=%d free=%d cb=%d pv=%p\n", j, i, RTHeapSimpleGetFreeSize(Heap), aOps[j].cb, aOps[j].pvAlloc);
            RTHeapSimpleFree(Heap, aOps[j].pvAlloc);
            aOps[j].pvAlloc = NULL;
            cFreed++;
        }
    }
    Assert(cFreed == RT_ELEMENTS(aOps));
    RTPrintf("i=done free=%d\n", RTHeapSimpleGetFreeSize(Heap));

    /* check that we're back at the right amount of free memory. */
    size_t cbAfter = RTHeapSimpleGetFreeSize(Heap);
    if (cbBefore != cbAfter)
    {
        RTPrintf("Warning: Either we've split out an alignment chunk at the start, or we've got\n"
                 "         an alloc/free accounting bug: cbBefore=%d cbAfter=%d\n", cbBefore, cbAfter);
        RTHeapSimpleDump(Heap, (PFNRTHEAPSIMPLEPRINTF)RTPrintf);
    }

    RTPrintf("tstHeapSimple: Success\n");
#ifdef LOG_ENABLED
    RTLogFlush(NULL);
#endif
    return 0;
}
