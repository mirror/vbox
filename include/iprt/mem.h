/** @file
 * innotek Portable Runtime - Memory Management and Manipulation.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

#ifndef ___iprt_mem_h
#define ___iprt_mem_h


#include <iprt/cdefs.h>
#include <iprt/types.h>

#ifdef IN_GC
# error "There are no RTMem APIs available Guest Context!"
#endif

__BEGIN_DECLS

/** @defgroup grp_rt_mem    RTMem - Memory Management and Manipulation
 * @ingroup grp_rt
 * @{
 */

/** @def RTMEM_ALIGNMENT
 * The alignment of the memory blocks returned by RTMemAlloc(), RTMemAllocZ(),
 * RTMemRealloc(), RTMemTmpAlloc() and RTMemTmpAllocZ() for allocations greater
 * than RTMEM_ALIGNMENT.
 */
#define RTMEM_ALIGNMENT    8

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
RTDECL(void *)  RTMemTmpAlloc(size_t cb);

/**
 * Allocates zero'ed temporary memory.
 *
 * Same as RTMemTmpAlloc() but the memory will be zero'ed.
 *
 * @returns Pointer to the allocated memory.
 * @returns NULL on failure.
 * @param   cb      Size in bytes of the memory block to allocated.
 */
RTDECL(void *)  RTMemTmpAllocZ(size_t cb);

/**
 * Free temporary memory.
 *
 * @param   pv      Pointer to memory block.
 */
RTDECL(void)    RTMemTmpFree(void *pv);


/**
 * Allocates memory.
 *
 * @returns Pointer to the allocated memory.
 * @returns NULL on failure.
 * @param   cb      Size in bytes of the memory block to allocated.
 */
RTDECL(void *)  RTMemAlloc(size_t cb);

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
RTDECL(void *)  RTMemAllocZ(size_t cb);

/**
 * Duplicates a chunk of memory into a new heap block.
 *
 * @returns New heap block with the duplicate data.
 * @returns NULL if we're out of memory.
 * @param   pvSrc   The memory to duplicate.
 * @param   cb      The amount of memory to duplicate.
 */
RTDECL(void *) RTMemDup(const void *pvSrc, size_t cb);

/**
 * Duplicates a chunk of memory into a new heap block with some
 * additional zeroed memory.
 *
 * @returns New heap block with the duplicate data.
 * @returns NULL if we're out of memory.
 * @param   pvSrc   The memory to duplicate.
 * @param   cbSrc   The amount of memory to duplicate.
 * @param   cbExtra The amount of extra memory to allocate and zero.
 */
RTDECL(void *) RTMemDupEx(const void *pvSrc, size_t cbSrc, size_t cbExtra);

/**
 * Reallocates memory.
 *
 * @returns Pointer to the allocated memory.
 * @returns NULL on failure.
 * @param   pvOld   The memory block to reallocate.
 * @param   cbNew   The new block size (in bytes).
 */
RTDECL(void *)  RTMemRealloc(void *pvOld, size_t cbNew);

/**
 * Free memory related to an virtual machine
 *
 * @param   pv      Pointer to memory block.
 */
RTDECL(void)    RTMemFree(void *pv);

/**
 * Allocates memory which may contain code.
 *
 * @returns Pointer to the allocated memory.
 * @returns NULL on failure.
 * @param   cb      Size in bytes of the memory block to allocate.
 */
RTDECL(void *)    RTMemExecAlloc(size_t cb);

/**
 * Free executable/read/write memory allocated by RTMemExecAlloc().
 *
 * @param   pv      Pointer to memory block.
 */
RTDECL(void)      RTMemExecFree(void *pv);

#if defined(IN_RING0) && defined(RT_ARCH_AMD64) && defined(RT_OS_LINUX)
/**
 * Donate read+write+execute memory to the exec heap.
 *
 * This API is specific to AMD64 and Linux/GNU. A kernel module that desires to
 * use RTMemExecAlloc on AMD64 Linux/GNU will have to donate some statically
 * allocated memory in the module if it wishes for GCC generated code to work.
 * GCC can only generate modules that work in the address range ~2GB to ~0
 * currently.
 *
 * The API only accept one single donation.
 *
 * @returns IPRT status code.
 * @param   pvMemory    Pointer to the memory block.
 * @param   cb          The size of the memory block.
 */
RTR0DECL(int) RTR0MemExecDonate(void *pvMemory, size_t cb);
#endif /* R0+AMD64+LINUX */

/**
 * Allocate page aligned memory.
 *
 * @returns Pointer to the allocated memory.
 * @returns NULL if we're out of memory.
 * @param   cb  Size of the memory block. Will be rounded up to page size.
 */
RTDECL(void *) RTMemPageAlloc(size_t cb);

/**
 * Allocate zero'ed page aligned memory.
 *
 * @returns Pointer to the allocated memory.
 * @returns NULL if we're out of memory.
 * @param   cb  Size of the memory block. Will be rounded up to page size.
 */
RTDECL(void *) RTMemPageAllocZ(size_t cb);

/**
 * Free a memory block allocated with RTMemPageAlloc() or RTMemPageAllocZ().
 *
 * @param   pv      Pointer to the block as it was returned by the allocation function.
 *                  NULL will be ignored.
 */
RTDECL(void) RTMemPageFree(void *pv);

/** Page level protection flags for RTMemProtect().
 * @{
 */
/** Read access. */
#define RTMEM_PROT_NONE   0
/** Read access. */
#define RTMEM_PROT_READ   1
/** Write access. */
#define RTMEM_PROT_WRITE  2
/** Execute access. */
#define RTMEM_PROT_EXEC   4
/** @} */

/**
 * Change the page level protection of a memory region.
 *
 * @returns iprt status code.
 * @param   pv          Start of the region. Will be rounded down to nearest page boundary.
 * @param   cb          Size of the region. Will be rounded up to the nearest page boundary.
 * @param   fProtect    The new protection, a combination of the RTMEM_PROT_* defines.
 */
RTDECL(int) RTMemProtect(void *pv, size_t cb, unsigned fProtect);


#ifdef IN_RING0

/**
 * Allocates physical contiguous memory (below 4GB).
 * The allocation is page aligned and the content is undefined.
 *
 * @returns Pointer to the memory block. This is page aligned.
 * @param   pPhys   Where to store the physical address.
 * @param   cb      The allocation size in bytes. This is always
 *                  rounded up to PAGE_SIZE.
 */
RTR0DECL(void *) RTMemContAlloc(PRTCCPHYS pPhys, size_t cb);

/**
 * Frees memory allocated ysing RTMemContAlloc().
 *
 * @param   pv      Pointer to return from RTMemContAlloc().
 * @param   cb      The cb parameter passed to RTMemContAlloc().
 */
RTR0DECL(void) RTMemContFree(void *pv, size_t cb);

#endif


/** @name Electrical Fence Version of some APIs.
 * @{
 */

/**
 * Same as RTMemTmpAlloc() except that it's fenced.
 *
 * @returns Pointer to the allocated memory.
 * @returns NULL on failure.
 * @param   cb      Size in bytes of the memory block to allocate.
 */
RTDECL(void *)  RTMemEfTmpAlloc(size_t cb);

/**
 * Same as RTMemTmpAllocZ() except that it's fenced.
 *
 * @returns Pointer to the allocated memory.
 * @returns NULL on failure.
 * @param   cb      Size in bytes of the memory block to allocate.
 */
RTDECL(void *)  RTMemEfTmpAllocZ(size_t cb);

/**
 * Same as RTMemTmpFree() except that it's for fenced memory.
 *
 * @param   pv      Pointer to memory block.
 */
RTDECL(void)    RTMemEfTmpFree(void *pv);

/**
 * Same as RTMemAlloc() except that it's fenced.
 *
 * @returns Pointer to the allocated memory. Free with RTMemEfFree().
 * @returns NULL on failure.
 * @param   cb      Size in bytes of the memory block to allocate.
 */
RTDECL(void *)  RTMemEfAlloc(size_t cb);

/**
 * Same as RTMemAllocZ() except that it's fenced.
 *
 * @returns Pointer to the allocated memory.
 * @returns NULL on failure.
 * @param   cb      Size in bytes of the memory block to allocate.
 */
RTDECL(void *)  RTMemEfAllocZ(size_t cb);

/**
 * Same as RTMemRealloc() except that it's fenced.
 *
 * @returns Pointer to the allocated memory.
 * @returns NULL on failure.
 * @param   pvOld   The memory block to reallocate.
 * @param   cbNew   The new block size (in bytes).
 */
RTDECL(void *)  RTMemEfRealloc(void *pvOld, size_t cbNew);

/**
 * Free memory allocated by any of the RTMemEf* allocators.
 *
 * @param   pv      Pointer to memory block.
 */
RTDECL(void)    RTMemEfFree(void *pv);

/**
 * Same as RTMemDup() except that it's fenced.
 *
 * @returns New heap block with the duplicate data.
 * @returns NULL if we're out of memory.
 * @param   pvSrc   The memory to duplicate.
 * @param   cb      The amount of memory to duplicate.
 */
RTDECL(void *) RTMemEfDup(const void *pvSrc, size_t cb);

/**
 * Same as RTMemEfDupEx except that it's fenced.
 *
 * @returns New heap block with the duplicate data.
 * @returns NULL if we're out of memory.
 * @param   pvSrc   The memory to duplicate.
 * @param   cbSrc   The amount of memory to duplicate.
 * @param   cbExtra The amount of extra memory to allocate and zero.
 */
RTDECL(void *) RTMemEfDupEx(const void *pvSrc, size_t cbSrc, size_t cbExtra);

/** @def RTMEM_WRAP_TO_EF_APIS
 * Define RTMEM_WRAP_TO_EF_APIS to wrap RTMem APIs to RTMemEf APIs.
 */
#ifdef RTMEM_WRAP_TO_EF_APIS
# define RTMemTmpAlloc  RTMemEfTmpAlloc
# define RTMemTmpAllocZ RTMemEfTmpAllocZ
# define RTMemTmpFree   RTMemEfTmpFree
# define RTMemAlloc     RTMemEfAlloc
# define RTMemAllocZ    RTMemEfAllocZ
# define RTMemRealloc   RTMemEfRealloc
# define RTMemFree      RTMemEfFree
# define RTMemDup       RTMemEfDup
# define RTMemDupEx     RTMemEfDupEx
#endif
#ifdef __DOXYGEN__
# define RTMEM_WRAP_TO_EF_APIS
#endif

/** @} */

/** @} */

__END_DECLS

#endif

