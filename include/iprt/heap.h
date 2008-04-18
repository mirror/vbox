/** @file
 * innotek Portable Runtime - A Simple Heap.
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

#ifndef ___iprt_heap_h
#define ___iprt_heap_h

#include <iprt/cdefs.h>
#include <iprt/types.h>

__BEGIN_DECLS

/**
 * Initializes the heap.
 *
 * @returns IPRT status code on success.
 * @param   pHeap       Where to store the heap anchor block on success.
 * @param   pvMemory    Pointer to the heap memory.
 * @param   cbMemory    The size of the heap memory.
 */
RTDECL(int) RTHeapSimpleInit(PRTHEAPSIMPLE pHeap, void *pvMemory, size_t cbMemory);

/**
 * Merge two simple heaps into one.
 *
 * The requiremet is of course that they next two each other memory wise.
 *
 * @returns IPRT status code on success.
 * @param   pHeap       Where to store the handle to the merged heap on success.
 * @param   Heap1       Handle to the first heap.
 * @param   Heap2       Handle to the second heap.
 * @remark  This API isn't implemented yet.
 */
RTDECL(int) RTHeapSimpleMerge(PRTHEAPSIMPLE pHeap, RTHEAPSIMPLE Heap1, RTHEAPSIMPLE Heap2);

/**
 * Allocates memory from the specified simple heap.
 *
 * @returns Pointer to the allocated memory block on success.
 * @returns NULL if the request cannot be satisfied. (A VERR_NO_MEMORY condition.)
 *
 * @param   Heap        The heap to allocate the memory on.
 * @param   cb          The requested heap block size.
 * @param   cbAlignment The requested heap block alignment. Pass 0 for default alignment.
 *                      Must be a power of 2.
 */
RTDECL(void *) RTHeapSimpleAlloc(RTHEAPSIMPLE Heap, size_t cb, size_t cbAlignment);

/**
 * Allocates zeroed memory from the specified simple heap.
 *
 * @returns Pointer to the allocated memory block on success.
 * @returns NULL if the request cannot be satisfied. (A VERR_NO_MEMORY condition.)
 *
 * @param   Heap        The heap to allocate the memory on.
 * @param   cb          The requested heap block size.
 * @param   cbAlignment The requested heap block alignment. Pass 0 for default alignment.
 *                      Must be a power of 2.
 */
RTDECL(void *) RTHeapSimpleAllocZ(RTHEAPSIMPLE Heap, size_t cb, size_t cbAlignment);

/**
 * Reallocates / Allocates / Frees a heap block.
 *
 * @param   Heap        The heap. This is optional and will only be used for strict assertions.
 * @param   pv          The heap block returned by RTHeapSimple. If NULL it behaves like RTHeapSimpleAlloc().
 * @param   cbNew       The new size of the heap block. If NULL it behaves like RTHeapSimpleFree().
 * @param   cbAlignment The requested heap block alignment. Pass 0 for default alignment.
 *                      Must be a power of 2.
 * @remark  This API isn't implemented yet.
 */
RTDECL(void *) RTHeapSimpleRealloc(RTHEAPSIMPLE Heap, void *pv, size_t cbNew, size_t cbAlignment);

/**
 * Reallocates / Allocates / Frees a heap block, zeroing any new bits.
 *
 * @param   Heap        The heap. This is optional and will only be used for strict assertions.
 * @param   pv          The heap block returned by RTHeapSimple. If NULL it behaves like RTHeapSimpleAllocZ().
 * @param   cbNew       The new size of the heap block. If NULL it behaves like RTHeapSimpleFree().
 * @param   cbAlignment The requested heap block alignment. Pass 0 for default alignment.
 *                      Must be a power of 2.
 * @remark  This API isn't implemented yet.
 */
RTDECL(void *) RTHeapSimpleReallocZ(RTHEAPSIMPLE Heap, void *pv, size_t cbNew, size_t cbAlignment);

/**
 * Frees memory allocated from a simple heap.
 *
 * @param   Heap    The heap. This is optional and will only be used for strict assertions.
 * @param   pv      The heap block returned by RTHeapSimple
 */
RTDECL(void) RTHeapSimpleFree(RTHEAPSIMPLE Heap, void *pv);

/**
 * Gets the size of the specified heap block.
 *
 * @returns The actual size of the heap block.
 * @returns 0 if \a pv is NULL or it doesn't point to a valid heap block. An invalid \a pv
 *          can also cause traps or trigger assertions.
 * @param   Heap    The heap. This is optional and will only be used for strict assertions.
 * @param   pv      The heap block returned by RTHeapSimple
 */
RTDECL(size_t) RTHeapSimpleSize(RTHEAPSIMPLE Heap, void *pv);

/**
 * Gets the size of the heap.
 *
 * This size includes all the internal heap structures. So, even if the heap is
 * empty the RTHeapSimpleGetFreeSize() will never reach the heap size returned
 * by this function.
 *
 * @returns The heap size.
 * @returns 0 if heap was safely detected as being bad.
 * @param   Heap    The heap.
 */
RTDECL(size_t) RTHeapSimpleGetHeapSize(RTHEAPSIMPLE Heap);

/**
 * Returns the sum of all free heap blocks.
 *
 * This is the amount of memory you can theoretically allocate
 * if you do allocations exactly matching the free blocks.
 *
 * @returns The size of the free blocks.
 * @returns 0 if heap was safely detected as being bad.
 * @param   Heap    The heap.
 */
RTDECL(size_t) RTHeapSimpleGetFreeSize(RTHEAPSIMPLE Heap);

/**
 * Printf like callbaclk function for RTHeapSimpleDump.
 * @param   pszFormat   IPRT format string.
 * @param   ...         Format arguments.
 */
typedef DECLCALLBACK(void) FNRTHEAPSIMPLEPRINTF(const char *pszFormat, ...);
/** Pointer to a FNRTHEAPSIMPLEPRINTF function. */
typedef FNRTHEAPSIMPLEPRINTF *PFNRTHEAPSIMPLEPRINTF;

/**
 * Dumps the hypervisor heap.
 *
 * @param   Heap        The heap handle.
 * @param   pfnPrintf   Printf like function that groks IPRT formatting.
 */
RTDECL(void) RTHeapSimpleDump(RTHEAPSIMPLE Heap, PFNRTHEAPSIMPLEPRINTF pfnPrintf);

__END_DECLS

#endif

