/** @file
 * innotek Portable Runtime - Parameter Definitions.
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

#ifndef ___iprt_param_h
#define ___iprt_param_h

/** @todo Much of the PAGE_* stuff here is obsolete and highly risky to have around.
 * As for component configs (MM_*), either we gather all in here or we move those bits away! */

/** @defgroup   grp_rt_param    System Parameter Definitions
 * @ingroup grp_rt_cdefs
 * @{
 */

/* Undefine PAGE_SIZE and PAGE_SHIFT to avoid unnecessary noice when clashing
   with system headers. Include system headers before / after iprt depending
   on which you wish to take precedence. */
#undef PAGE_SIZE
#undef PAGE_SHIFT

/**
 * i386 Page size.
 */
#define PAGE_SIZE           4096

/**
 * i386 Page shift.
 * This is used to convert between size (in bytes) and page count.
 */
#define PAGE_SHIFT          12

/**
 * i386 Page offset mask.
 *
 * Do NOT one-complement this for whatever purpose. You may get a 32-bit const when you want a 64-bit one.
 * Use PAGE_BASE_MASK, PAGE_BASE_GC_MASK, PAGE_BASE_HC_MASK, PAGE_ADDRESS() or X86_PTE_PAE_PG_MASK.
 */
#define PAGE_OFFSET_MASK    0xfff

/**
 * Page address mask for the guest context POINTERS.
 * @remark  Physical addresses are always masked using X86_PTE_PAE_PG_MASK!
 */
#define PAGE_BASE_GC_MASK   (~(RTGCUINTPTR)0xfff)

/**
 * Page address mask for the host context POINTERS.
 * @remark  Physical addresses are always masked using X86_PTE_PAE_PG_MASK!
 */
#define PAGE_BASE_HC_MASK   (~(RTHCUINTPTR)0xfff)

/**
 * Page address mask for the both context POINTERS.
 *
 * Be careful when using this since it may be a size too big!
 * @remark  Physical addresses are always masked using X86_PTE_PAE_PG_MASK!
 */
#define PAGE_BASE_MASK     (~(RTUINTPTR)0xfff)

/**
 * Get the page aligned address of a POINTER in the CURRENT context.
 *
 * @returns Page aligned address (it's an uintptr_t).
 * @param   pv      The virtual address to align.
 *
 * @remarks Physical addresses are always masked using X86_PTE_PAE_PG_MASK!
 * @remarks This only works with POINTERS in the current context.
 *          Do NOT use on guest address or physical address!
 */
#define PAGE_ADDRESS(pv)    ((uintptr_t)(pv) & ~(uintptr_t)0xfff)

/**
 * Get the page aligned address of a physical address
 *
 * @returns Page aligned address (it's an RTHCPHYS or RTGCPHYS).
 * @param   Phys    The physical address to align.
 */
#define PHYS_PAGE_ADDRESS(Phys) ((Phys) & X86_PTE_PAE_PG_MASK)

/**
 * Host max path (the reasonable value).
 */
#define RTPATH_MAX   (4096 + 4)      /* (PATH_MAX + 1) on linux w/ some alignment */

/** @} */


/** @} */

#endif

