/* $Id$ */
/** @file
 * Incredibly Portable Runtime - Memory Allocation.
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
#include "alloc-ef.h"
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/param.h>
#include <iprt/string.h>

#include <stdlib.h>


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
 * @param   cb      Size in bytes of the memory block to allocate.
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
 * @param   cb      Size in bytes of the memory block to allocate.
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
    RTMemFree(pv);
}


/**
 * Allocates memory.
 *
 * @returns Pointer to the allocated memory.
 * @returns NULL on failure.
 * @param   cb      Size in bytes of the memory block to allocate.
 */
RTDECL(void *)  RTMemAlloc(size_t cb)
{
#ifdef RTALLOC_USE_EFENCE
    void *pv = rtMemAlloc("Alloc", RTMEMTYPE_RTMEMALLOC, cb, ((void **)&cb)[-1], 0, NULL, NULL);

#else /* !RTALLOC_USE_EFENCE */

    AssertMsg(cb, ("Allocating ZERO bytes is really not a good idea! Good luck with the next assertion!\n"));
    void *pv = malloc(cb);
    AssertMsg(pv, ("malloc(%d) failed!!!\n", cb));
#ifdef RT_OS_OS2 /* temporary workaround until libc062. */
    AssertMsg(   cb < 32
              || !((uintptr_t)pv & (RTMEM_ALIGNMENT - 1)), ("pv=%p RTMEM_ALIGNMENT=%#x\n", pv, RTMEM_ALIGNMENT));
#else
    AssertMsg(   cb < RTMEM_ALIGNMENT
              || !((uintptr_t)pv & (RTMEM_ALIGNMENT - 1)), ("pv=%p RTMEM_ALIGNMENT=%#x\n", pv, RTMEM_ALIGNMENT));
#endif
#endif /* !RTALLOC_USE_EFENCE */
    return pv;
}


/**
 * Allocates zero'ed memory.
 *
 * Instead of memset(pv, 0, sizeof()) use this when you want zero'ed
 * memory. This keeps the code smaller and the heap can skip the memset
 * in about 0.42% of the calls :-).
 *
 * @returns Pointer to the allocated memory.
 * @returns NULL on failure.
 * @param   cb      Size in bytes of the memory block to allocate.
 */
RTDECL(void *)  RTMemAllocZ(size_t cb)
{
#ifdef RTALLOC_USE_EFENCE
    void *pv = rtMemAlloc("AllocZ", RTMEMTYPE_RTMEMALLOCZ, cb, ((void **)&cb)[-1], 0, NULL, NULL);

#else /* !RTALLOC_USE_EFENCE */

    AssertMsg(cb, ("Allocating ZERO bytes is really not a good idea! Good luck with the next assertion!\n"));

    void *pv = calloc(1, cb);
    AssertMsg(pv, ("calloc(1,%d) failed!!!\n", cb));
#ifdef RT_OS_OS2 /* temporary workaround until libc062. */
    AssertMsg(   cb < 32
              || !((uintptr_t)pv & (RTMEM_ALIGNMENT - 1)), ("pv=%p RTMEM_ALIGNMENT=%#x\n", pv, RTMEM_ALIGNMENT));
#else
    AssertMsg(   cb < RTMEM_ALIGNMENT
              || !((uintptr_t)pv & (RTMEM_ALIGNMENT - 1)), ("pv=%p RTMEM_ALIGNMENT=%#x\n", pv, RTMEM_ALIGNMENT));
#endif
#endif /* !RTALLOC_USE_EFENCE */
    return pv;
}


/**
 * Reallocates memory.
 *
 * @returns Pointer to the allocated memory.
 * @returns NULL on failure.
 * @param   pvOld   The memory block to reallocate.
 * @param   cbNew   The new block size (in bytes).
 */
RTDECL(void *)  RTMemRealloc(void *pvOld, size_t cbNew)
{
#ifdef RTALLOC_USE_EFENCE
    void *pv = rtMemRealloc("Realloc", RTMEMTYPE_RTMEMREALLOC, pvOld, cbNew, ((void **)&pvOld)[-1], 0, NULL, NULL);

#else /* !RTALLOC_USE_EFENCE */

    void *pv = realloc(pvOld, cbNew);
    AssertMsg(pv && cbNew, ("realloc(%p, %d) failed!!!\n", pvOld, cbNew));
#ifdef RT_OS_OS2 /* temporary workaround until libc062. */
    AssertMsg(   cbNew < 32
              || !((uintptr_t)pv & (RTMEM_ALIGNMENT - 1)), ("pv=%p RTMEM_ALIGNMENT=%#x\n", pv, RTMEM_ALIGNMENT));
#else
    AssertMsg(   cbNew < RTMEM_ALIGNMENT
              || !((uintptr_t)pv & (RTMEM_ALIGNMENT - 1)), ("pv=%p RTMEM_ALIGNMENT=%#x\n", pv, RTMEM_ALIGNMENT));
#endif
#endif  /* !RTALLOC_USE_EFENCE */
    return pv;
}


/**
 * Free memory related to a virtual machine
 *
 * @param   pv      Pointer to memory block.
 */
RTDECL(void)    RTMemFree(void *pv)
{
    if (pv)
#ifdef RTALLOC_USE_EFENCE
        rtMemFree("Free", RTMEMTYPE_RTMEMFREE, pv, ((void **)&pv)[-1], 0, NULL, NULL);
#else
        free(pv);
#endif
}

