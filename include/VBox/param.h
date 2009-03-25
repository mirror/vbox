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
#if HC_ARCH_BITS == 64 && defined(VBOX_WITH_NEW_PHYS_CODE)
# define MM_RAM_MAX                 UINT64_C(0x400000000)
#else
# define MM_RAM_MAX                 UINT64_C(0x0E0000000)
#endif
/** The minimum guest RAM size in MBs. */
#define MM_RAM_MIN_IN_MB            UINT32_C(4)
/** The maximum guest RAM size in MBs. */
#if HC_ARCH_BITS == 64 && defined(VBOX_WITH_NEW_PHYS_CODE)
# define MM_RAM_MAX_IN_MB           UINT32_C(16384)
#else
# define MM_RAM_MAX_IN_MB           UINT32_C(3584)
#endif
/** The default size of the below 4GB RAM hole. */
#define MM_RAM_HOLE_SIZE_DEFAULT    (512U * _1M)

/** @} */


/** @defgroup   grp_vbox_param_vmm  VMM Parameters
 * @ingroup grp_vbox_param
 * @{
 */

/** VMM stack size. */
#define VMM_STACK_SIZE              8192U

/** @} */


/** @} */

#endif

