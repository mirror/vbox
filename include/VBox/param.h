/** @file
 * VirtualBox Parameter Definitions.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBox_param_h
#define ___VBox_param_h

#include <iprt/param.h>


/** @defgroup   grp_vbox_param  VBox Parameter Definition
 * @{
 */


/** @defgroup   grp_vbox_param_mm  Memory Monitor Parameters
 * @ingroup grp_vbox_param
 * @{
 */

/** Initial address of Hypervisor Memory Area.
 * MUST BE PAGE TABLE ALIGNED! */
#define MM_HYPER_AREA_ADDRESS   0xa0000000

/** The max size of the hypervisor memory area. */
#define MM_HYPER_AREA_MAX_SIZE (12*1024*1024)

/** Maximum number of bytes we can dynamically map into the hypervisor region.
 * This must be a power of 2 number of pages!
 */
#define MM_HYPER_DYNAMIC_SIZE   (8 * PAGE_SIZE)

/** @} */


/** @defgroup   grp_vbox_param_vmm  VMM Parameters
 * @ingroup grp_vbox_param
 * @{
 */

/** VMM stack size. */
#define VMM_STACK_SIZE    8192

/** @} */


/** @} */

#endif

