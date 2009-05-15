/** @file
 * VirtualBox Parameter Definitions.
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

#ifndef ___VBox_param_h
#define ___VBox_param_h

#include <iprt/param.h>
#include <iprt/cdefs.h>


/** @defgroup   grp_vbox_param  VBox Parameter Definition
 * @{
 */

/** The maximum number of pages that can be allocated and mapped
 * by various MM, PGM and SUP APIs. */
#define VBOX_MAX_ALLOC_PAGE_COUNT   (128U * _1M / PAGE_SIZE)


/** @defgroup   grp_vbox_param_mm  Memory Monitor Parameters
 * @ingroup grp_vbox_param
 * @{
 */
/** Initial address of Hypervisor Memory Area.
 * MUST BE PAGE TABLE ALIGNED! */
#define MM_HYPER_AREA_ADDRESS       UINT32_C(0xa0000000)

/** The max size of the hypervisor memory area. */
#define MM_HYPER_AREA_MAX_SIZE      (40U * _1M) /**< @todo Readjust when floating RAMRANGEs have been implemented. Used to be 20 * _1MB */

/** Maximum number of bytes we can dynamically map into the hypervisor region.
 * This must be a power of 2 number of pages!
 */
#define MM_HYPER_DYNAMIC_SIZE       (16U * PAGE_SIZE)

/** The minimum guest RAM size in bytes. */
#define MM_RAM_MIN                  UINT32_C(0x00400000)
/** The maximum guest RAM size in bytes. */
#if HC_ARCH_BITS == 64
# define MM_RAM_MAX                 UINT64_C(0x400000000)
#else
# define MM_RAM_MAX                 UINT64_C(0x0E0000000)
#endif
/** The minimum guest RAM size in MBs. */
#define MM_RAM_MIN_IN_MB            UINT32_C(4)
/** The maximum guest RAM size in MBs. */
#if HC_ARCH_BITS == 64
# define MM_RAM_MAX_IN_MB           UINT32_C(16384)
#else
# define MM_RAM_MAX_IN_MB           UINT32_C(3584)
#endif
/** The default size of the below 4GB RAM hole. */
#define MM_RAM_HOLE_SIZE_DEFAULT    (512U * _1M)
/** @} */


/** @defgroup   grp_vbox_param_pgm  Page Manager Parameters
 * @ingroup grp_vbox_param
 * @{
 */
/** The number of handy pages.
 * This should be a power of two. */
#define PGM_HANDY_PAGES             128
/** The threshold at which allocation of more handy pages is flagged. */
#define PGM_HANDY_PAGES_SET_FF      32
/** The threshold at which we will allocate more when in ring-3.
 * This is must be smaller than both PGM_HANDY_PAGES_SET_FF and
 * PGM_HANDY_PAGES_MIN. */
#define PGM_HANDY_PAGES_R3_ALLOC    8
/** The threshold at which we will allocate more when in ring-0 or raw mode.
 * The idea is that we should never go below this threshold while in ring-0 or
 * raw mode because of PGM_HANDY_PAGES_RZ_TO_R3. However, should this happen and
 * we are actually out of memory, we will have 8 page to get out of whatever
 * code we're executing.
 *
 * This is must be smaller than both PGM_HANDY_PAGES_SET_FF and
 * PGM_HANDY_PAGES_MIN. */
#define PGM_HANDY_PAGES_RZ_ALLOC    8
/** The threshold at which we force return to R3 ASAP.
 * The idea is that this should be large enough to get out of any code and up to
 * the main EM loop when we are out of memory.
 * This must be less or equal to PGM_HANDY_PAGES_MIN. */
#define PGM_HANDY_PAGES_RZ_TO_R3    24
/** The minimum number of handy pages (after allocation).
 * This must be greater or equal to PGM_HANDY_PAGES_SET_FF.
 * Another name would be PGM_HANDY_PAGES_EXTRA_RESERVATION or _PARANOIA. :-) */
#define PGM_HANDY_PAGES_MIN         32
/** @} */


/** @defgroup   grp_vbox_param_vmm  VMM Parameters
 * @ingroup grp_vbox_param
 * @{
 */
/** VMM stack size. */
#define VMM_STACK_SIZE              8192U
/** Min number of Virtual CPUs. */
#define VMM_MIN_CPU_COUNT           1
/** Max number of Virtual CPUs. */
#define VMM_MAX_CPU_COUNT           32

/** @} */


/** @} */

#endif

