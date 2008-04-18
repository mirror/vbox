/* $Id$ */
/** @file
 * innotek Portable Runtime - Include all necessary headers for the NT kernel.
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

#ifndef ___the_nt_kernel_h
#define ___the_nt_kernel_h

#include <iprt/cdefs.h>

#if (_MSC_VER >= 1400) && !defined(VBOX_WITH_PATCHED_DDK)
# include <iprt/asm.h>
# define _InterlockedExchange           _InterlockedExchange_StupidDDKVsCompilerCrap
# define _InterlockedExchangeAdd        _InterlockedExchangeAdd_StupidDDKVsCompilerCrap
# define _InterlockedCompareExchange    _InterlockedCompareExchange_StupidDDKVsCompilerCrap
# define _InterlockedAddLargeStatistic  _InterlockedAddLargeStatistic_StupidDDKVsCompilerCrap
__BEGIN_DECLS
# include <ntddk.h>
__END_DECLS
# undef  _InterlockedExchange
# undef  _InterlockedExchangeAdd
# undef  _InterlockedCompareExchange
# undef  _InterlockedAddLargeStatistic
#else
__BEGIN_DECLS
# include <ntddk.h>
__END_DECLS
#endif

#include <memory.h>
#if !defined(RT_OS_WINDOWS)
# error "RT_OS_WINDOWS must be defined!"
#endif

#include <iprt/param.h>
#ifndef PAGE_OFFSET_MASK
# define PAGE_OFFSET_MASK (PAGE_SIZE - 1)
#endif

/*
 * When targeting NT4 we have to undo some of the nice macros
 * installed by the later DDKs.
 */
#ifdef IPRT_TARGET_NT4
# undef ExAllocatePoolWithTag
# define ExAllocatePoolWithTag(a,b,c) ExAllocatePool(a,b)
# undef ExAllocatePoolWithQuotaTag
# define ExAllocatePoolWithQuotaTag(a,b,c) ExAllocatePoolWithQuota(a,b)
# undef ExAllocatePool
  NTKERNELAPI PVOID NTAPI ExAllocatePool(IN POOL_TYPE PoolType, IN SIZE_T NumberOfBytes);
# undef ExFreePool
  NTKERNELAPI VOID NTAPI ExFreePool(IN PVOID P);
#endif /* IPRT_TARGET_NT4 */

/** @def IPRT_NT_POOL_TAG
 * Tag to use with the NT Pool APIs.
 * In memory and in the various windbg tool it appears in the reverse order of
 * what it is given as here, so it'll read "IPRT".
 */
#define IPRT_NT_POOL_TAG    'TRPI'

#endif

