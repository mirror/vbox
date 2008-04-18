/* $Id$ */
/** @file
 * Incredibly Portable Runtime - Memory Allocation, electric fence.
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

#ifndef ___alloc_ef_h
#define ___alloc_ef_h

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#if defined(__DOXYGEN__)
# define RTALLOC_USE_EFENCE
# define RTALLOC_EFENCE_IN_FRONT
# define RTALLOC_EFENCE_FREE_FILL 'f'
#endif

/** @def RTALLOC_USE_EFENCE
 * If defined the electric fence put up for ALL allocations by RTMemAlloc(),
 * RTMemAllocZ(), RTMemRealloc(), RTMemTmpAlloc() and RTMemTmpAllocZ().
 */
#if 0// defined(DEBUG_bird)
# define RTALLOC_USE_EFENCE
#endif

/** @def RTALLOC_EFENCE_SIZE
 * The size of the fence. This must be page aligned.
 */
#define RTALLOC_EFENCE_SIZE             PAGE_SIZE

/** @def RTALLOC_EFENCE_IN_FRONT
 * Define this to put the fence up in front of the block.
 * The default (when this isn't defined) is to up it up after the block.
 */
//# define RTALLOC_EFENCE_IN_FRONT

/** @def RTALLOC_EFENCE_TRACE
 * Define this to support actual free and reallocation of blocks.
 */
#define RTALLOC_EFENCE_TRACE

/** @def RTALLOC_EFENCE_FREE_DELAYED
 * This define will enable free() delay and protection of the freed data
 * while it's being delayed. The value of RTALLOC_EFENCE_FREE_DELAYED defines
 * the threshold of the delayed blocks.
 * Delayed blocks does not consume any physical memory, only virtual address space.
 * Requires RTALLOC_EFENCE_TRACE.
 */
#define RTALLOC_EFENCE_FREE_DELAYED     (20 * _1M)

/** @def RTALLOC_EFENCE_FREE_FILL
 * This define will enable memset(,RTALLOC_EFENCE_FREE_FILL,)'ing the user memory
 * in the block before freeing/decommitting it. This is useful in GDB since GDB
 * appeares to be able to read the content of the page even after it's been
 * decommitted.
 * Requires RTALLOC_EFENCE_TRACE.
 */
#if defined(RT_OS_LINUX)
# define RTALLOC_EFENCE_FREE_FILL       'f'
#endif

/** @def RTALLOC_EFENCE_FILLER
 * This define will enable memset(,RTALLOC_EFENCE_FILLER,)'ing the allocated
 * memory when the API doesn't require it to be zero'ed.
 */
#define RTALLOC_EFENCE_FILLER           0xef

#if defined(__DOXYGEN__)
/** @def RTALLOC_EFENCE_CPP
 * This define will enable the new and delete wrappers.
 */
# define RTALLOC_EFENCE_CPP
#endif



/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#ifdef RT_OS_WINDOWS
# include <Windows.h>
#else
# include <sys/mman.h>
#endif
#include <iprt/avl.h>
#include <iprt/thread.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Allocation types.
 */
typedef enum RTMEMTYPE
{
    RTMEMTYPE_RTMEMALLOC,
    RTMEMTYPE_RTMEMALLOCZ,
    RTMEMTYPE_RTMEMREALLOC,
    RTMEMTYPE_RTMEMFREE,

    RTMEMTYPE_NEW,
    RTMEMTYPE_NEW_ARRAY,
    RTMEMTYPE_DELETE,
    RTMEMTYPE_DELETE_ARRAY
} RTMEMTYPE;

#ifdef RTALLOC_EFENCE_TRACE
/**
 * Node tracking a memory allocation.
 */
typedef struct RTMEMBLOCK
{
    /** Avl node code, key is the user block pointer. */
    AVLPVNODECORE   Core;
    /** Allocation type. */
    RTMEMTYPE       enmType;
    /** The size of the block. */
    size_t          cb;
    /** The return address of the allocator function. */
    void           *pvCaller;
    /** Line number of the alloc call. */
    unsigned        iLine;
    /** File from within the allocation was made. */
    const char     *pszFile;
    /** Function from within the allocation was made. */
    const char     *pszFunction;
} RTMEMBLOCK, *PRTMEMBLOCK;

#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
__BEGIN_DECLS
RTDECL(void *) rtMemAlloc(const char *pszOp, RTMEMTYPE enmType, size_t cb, void *pvCaller, unsigned iLine, const char *pszFile, const char *pszFunction);
RTDECL(void *) rtMemRealloc(const char *pszOp, RTMEMTYPE enmType, void *pvOld, size_t cbNew, void *pvCaller, unsigned iLine, const char *pszFile, const char *pszFunction);
RTDECL(void) rtMemFree(const char *pszOp, RTMEMTYPE enmType, void *pv, void *pvCaller, unsigned iLine, const char *pszFile, const char *pszFunction);
__END_DECLS

#endif

