/* $Id$ */
/** @file
 * IPRT - Memory Allocation.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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
*   Defined Constants And Macros                                               *
*******************************************************************************/
#ifdef RTMEM_WRAP_TO_EF_APIS
# undef RTMEM_WRAP_TO_EF_APIS
# define RTALLOC_USE_EFENCE 1
#endif


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "alloc-ef.h"
#include <iprt/alloc.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/param.h>
#include <iprt/string.h>

#include <stdlib.h>

#undef RTMemTmpAlloc
#undef RTMemTmpAllocTag
#undef RTMemTmpAllocZ
#undef RTMemTmpAllocZTag
#undef RTMemTmpFree
#undef RTMemAlloc
#undef RTMemAllocTag
#undef RTMemAllocZ
#undef RTMemAllocZTag
#undef RTMemAllocVar
#undef RTMemAllocVarTag
#undef RTMemAllocZVar
#undef RTMemAllocZVarTag
#undef RTMemRealloc
#undef RTMemReallocTag
#undef RTMemFree
#undef RTMemDup
#undef RTMemDupTag
#undef RTMemDupEx
#undef RTMemDupExTag


RTDECL(void *)  RTMemTmpAllocTag(size_t cb, const char *pszTag) RT_NO_THROW
{
    return RTMemAllocTag(cb, pszTag);
}


RTDECL(void *)  RTMemTmpAllocZTag(size_t cb, const char *pszTag) RT_NO_THROW
{
    return RTMemAllocZTag(cb, pszTag);
}


RTDECL(void) RTMemTmpFree(void *pv) RT_NO_THROW
{
    RTMemFree(pv);
}


RTDECL(void *) RTMemAllocTag(size_t cb, const char *pszTag) RT_NO_THROW
{
#ifdef RTALLOC_USE_EFENCE
    void *pv = rtR3MemAlloc("Alloc", RTMEMTYPE_RTMEMALLOC, cb, cb, pszTag, ASMReturnAddress(), NULL, 0, NULL);

#else /* !RTALLOC_USE_EFENCE */

    AssertMsg(cb, ("Allocating ZERO bytes is really not a good idea! Good luck with the next assertion!\n"));
    void *pv = malloc(cb);
    AssertMsg(pv, ("malloc(%#zx) failed!!!\n", cb));
    AssertMsg(   cb < RTMEM_ALIGNMENT
              || !((uintptr_t)pv & (RTMEM_ALIGNMENT - 1))
              || ( (cb & RTMEM_ALIGNMENT) + ((uintptr_t)pv & RTMEM_ALIGNMENT)) == RTMEM_ALIGNMENT
              , ("pv=%p RTMEM_ALIGNMENT=%#x\n", pv, RTMEM_ALIGNMENT));
#endif /* !RTALLOC_USE_EFENCE */
    return pv;
}


RTDECL(void *) RTMemAllocZTag(size_t cb, const char *pszTag) RT_NO_THROW
{
#ifdef RTALLOC_USE_EFENCE
    void *pv = rtR3MemAlloc("AllocZ", RTMEMTYPE_RTMEMALLOCZ, cb, cb, pszTag, ASMReturnAddress(), NULL, 0, NULL);

#else /* !RTALLOC_USE_EFENCE */

    AssertMsg(cb, ("Allocating ZERO bytes is really not a good idea! Good luck with the next assertion!\n"));

    void *pv = calloc(1, cb);
    AssertMsg(pv, ("calloc(1,%#zx) failed!!!\n", cb));
    AssertMsg(   cb < RTMEM_ALIGNMENT
              || !((uintptr_t)pv & (RTMEM_ALIGNMENT - 1))
              || ( (cb & RTMEM_ALIGNMENT) + ((uintptr_t)pv & RTMEM_ALIGNMENT)) == RTMEM_ALIGNMENT
              , ("pv=%p RTMEM_ALIGNMENT=%#x\n", pv, RTMEM_ALIGNMENT));
#endif /* !RTALLOC_USE_EFENCE */
    return pv;
}


RTDECL(void *) RTMemAllocVarTag(size_t cbUnaligned, const char *pszTag) RT_NO_THROW
{
    size_t cbAligned;
    if (cbUnaligned >= 16)
        cbAligned = RT_ALIGN_Z(cbUnaligned, 16);
    else
        cbAligned = RT_ALIGN_Z(cbUnaligned, sizeof(void *));
#ifdef RTALLOC_USE_EFENCE
    void *pv = rtR3MemAlloc("AllocVar", RTMEMTYPE_RTMEMALLOC, cbUnaligned, cbAligned, pszTag, ASMReturnAddress(), NULL, 0, NULL);
#else
    void *pv = RTMemAllocTag(cbAligned, pszTag);
#endif
    return pv;
}


RTDECL(void *) RTMemAllocZVarTag(size_t cbUnaligned, const char *pszTag) RT_NO_THROW
{
    size_t cbAligned;
    if (cbUnaligned >= 16)
        cbAligned = RT_ALIGN_Z(cbUnaligned, 16);
    else
        cbAligned = RT_ALIGN_Z(cbUnaligned, sizeof(void *));
#ifdef RTALLOC_USE_EFENCE
    void *pv = rtR3MemAlloc("AllocZVar", RTMEMTYPE_RTMEMALLOCZ, cbUnaligned, cbAligned, pszTag, ASMReturnAddress(), NULL, 0, NULL);
#else
    void *pv = RTMemAllocZTag(cbAligned, pszTag);
#endif
    return pv;
}


RTDECL(void *)  RTMemReallocTag(void *pvOld, size_t cbNew, const char *pszTag) RT_NO_THROW
{
#ifdef RTALLOC_USE_EFENCE
    void *pv = rtR3MemRealloc("Realloc", RTMEMTYPE_RTMEMREALLOC, pvOld, cbNew, pszTag, ASMReturnAddress(), NULL, 0, NULL);

#else /* !RTALLOC_USE_EFENCE */

    void *pv = realloc(pvOld, cbNew);
    AssertMsg(pv && cbNew, ("realloc(%p, %#zx) failed!!!\n", pvOld, cbNew));
    AssertMsg(   cbNew < RTMEM_ALIGNMENT
              || !((uintptr_t)pv & (RTMEM_ALIGNMENT - 1))
              || ( (cbNew & RTMEM_ALIGNMENT) + ((uintptr_t)pv & RTMEM_ALIGNMENT)) == RTMEM_ALIGNMENT
              , ("pv=%p RTMEM_ALIGNMENT=%#x\n", pv, RTMEM_ALIGNMENT));
#endif  /* !RTALLOC_USE_EFENCE */
    return pv;
}


RTDECL(void) RTMemFree(void *pv) RT_NO_THROW
{
    if (pv)
#ifdef RTALLOC_USE_EFENCE
        rtR3MemFree("Free", RTMEMTYPE_RTMEMFREE, pv, ASMReturnAddress(), NULL, 0, NULL);
#else
        free(pv);
#endif
}

