/* $Id: $ */
/** @file
 * InnoTek Portable Runtime - A Simple Heap.
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


/* >>> heap.h */
#ifndef __iprt_heap_h__
#define __iprt_heap_h__

#include <iprt/cdefs.h>
#include <iprt/types.h>

/* >>> types.h */
/** Handle to a simple heap. */
typedef struct RTHEAPSIMPLEINTERNAL *RTHEAPSIMPLE;
/** Pointer to a handle to a simple heap. */
typedef RTHEAPSIMPLE *PRTHEAPSIMPLE;
/* <<< types.h */

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
 * Frees memory allocated from a simple heap.
 * 
 * @param   Heap    The heap. This is optional and will only be used for strict assertions.
 * @param   pv      The heap block returned by RTHeapSimple
 */
RTDECL(void) RTHeapSimpleFree(RTHEAPSIMPLE Heap, void *pv);

#ifdef DEBUG
/**
 * Dumps the hypervisor heap to the (default) log.
 * 
 * @param   Heap        The heap handle.
 */
RTDECL(void) RTHeapSimpleDump(RTHEAPSIMPLE Heap);
#endif


__END_DECLS

#endif 

