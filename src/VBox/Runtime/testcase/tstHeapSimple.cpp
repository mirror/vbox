/* $Id $ */
/** @file
 * InnoTek Portable Runtime Testcase - Simple Heap.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
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
        {        16,          0,    NULL,  0 },
        {        16,          4,    NULL,  1 },
        {        16,          8,    NULL,  2 },
        {        16,         16,    NULL,  5 },
        {        16,         32,    NULL,  4 },
        {        32,          0,    NULL,  3 },
        {        31,          0,    NULL,  6 },
        {      1024,          0,    NULL,  8 },
        {      1024,         32,    NULL, 10 },
        {      1024,         32,    NULL, 12 },
        { PAGE_SIZE,  PAGE_SIZE,    NULL, 13 },
        {      1024,         32,    NULL,  9 },
        { PAGE_SIZE,         32,    NULL, 11 },
        { PAGE_SIZE,  PAGE_SIZE,    NULL, 14 },
        {        16,          0,    NULL, 15 },
        {        9,           0,    NULL,  7 },
        {        16,          0,    NULL,  7 },
        {        36,          0,    NULL,  7 },
        {        16,          0,    NULL,  7 },
        {     12344,          0,    NULL,  7 },
        {        50,          0,    NULL,  7 },
        {        16,          0,    NULL,  7 },
    };
    unsigned i;
#ifdef DEBUG
    RTHeapSimpleDump(Heap);
#endif
//    size_t cbBefore = RTHeapSimpleGetFreeSize(Heap);
    size_t cbBefore = 0;
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
        if (ALIGNP(aOps[i].pvAlloc, (aOps[i].uAlignment ? aOps[i].uAlignment : 8)) != aOps[i].pvAlloc)
        {
            RTPrintf("Failure: RTHeapSimpleAlloc(%p, %#x, %#x,) -> %p\n", (void *)Heap, aOps[i].cb, aOps[i].uAlignment, i);
            return 1;
        }
    }

    /* free and allocate the same node again. */
    for (i = 0; i < ELEMENTS(aOps); i++)
    {
        if (    !aOps[i].pvAlloc
            ||  aOps[i].uAlignment == PAGE_SIZE)
            continue;
        //size_t cbBeforeSub = RTHeapSimpleGetFreeSize(Heap);
        RTHeapSimpleFree(Heap, aOps[i].pvAlloc);

        //RTPrintf("debug: i=%d cbBeforeSub=%d now=%d\n", i, cbBeforeSub, RTHeapSimpleGetFreeSize(Heap));
        void *pv;
        pv = RTHeapSimpleAlloc(Heap, aOps[i].cb, aOps[i].uAlignment);
        if (pv)
        {
            RTPrintf("Failure: RTHeapSimpleAlloc(%p, %#x, %#x,) -> NULL i=%d\n", (void *)Heap, aOps[i].cb, aOps[i].uAlignment, i);
            return 1;
        }
        if (pv != aOps[i].pvAlloc)
        {
            RTPrintf("Failure: Free+Alloc returned different address. new=%p old=%p i=%d (doesn't work with delayed free)\n", pv, aOps[i].pvAlloc, i);
            //return 1;
        }
        aOps[i].pvAlloc = pv;
        #if 0 /* won't work :/ */
        size_t cbAfterSub = RTHeapSimpleGetFreeSize(Heap);
        if (cbBeforeSub != cbAfterSub)
        {
            RTPrintf("Failure: cbBeforeSub=%d cbAfterSub=%d. i=%d\n", cbBeforeSub, cbAfterSub, i);
            return 1;
        }
        #endif
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
            if (aOps[j].uAlignment == PAGE_SIZE)
                cbBefore -= aOps[j].cb;
            else
                RTHeapSimpleFree(Heap, aOps[j].pvAlloc);
            aOps[j].pvAlloc = NULL;
            cFreed++;
        }
    }
    Assert(cFreed == RT_ELEMENTS(aOps));
    //RTPrintf("i=done free=%d\n", RTHeapSimpleGetFreeSize(Heap));

#if 0
    /* check that we're back at the right amount of free memory. */
    //size_t cbAfter = RTHeapSimpleGetFreeSize(Heap);
    if (cbBefore != cbAfter)
    {
        RTPrintf("Warning: Either we've split out an alignment chunk at the start, or we've got\n"
                 "         an alloc/free accounting bug: cbBefore=%d cbAfter=%d\n", cbBefore, cbAfter);
#ifdef DEBUG
        RTHeapSimpleDump(Heap);
#endif
    }
#endif 

    RTPrintf("tstHeapSimple: Success\n");
#ifdef LOG_ENABLED
    RTLogFlush(NULL);
#endif
    return 0;
}
