/* $Id$ */
/** @file
 * DBGPlugInLinux - Instantiate LNX_TEMPLATE_HEADER for all different struct module versions.
 */

/*
 * Copyright (C) 2019-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*
 * Newest first so the list walker can select the right instance.
 */

#define LNX_VER     LNX_MK_VER(2,6,27)
#define LNX_SUFFIX  RT_CONCAT(_2_6_27,LNX_BIT_SUFFIX)
#include LNX_TEMPLATE_HEADER

#define LNX_VER     LNX_MK_VER(2,6,25)
#define LNX_SUFFIX  RT_CONCAT(_2_6_25,LNX_BIT_SUFFIX)
#include LNX_TEMPLATE_HEADER

#define LNX_VER     LNX_MK_VER(2,6,24)
#define LNX_SUFFIX  RT_CONCAT(_2_6_24,LNX_BIT_SUFFIX)
#include LNX_TEMPLATE_HEADER

#define LNX_VER     LNX_MK_VER(2,6,21)
#define LNX_SUFFIX  RT_CONCAT(_2_6_21,LNX_BIT_SUFFIX)
#include LNX_TEMPLATE_HEADER

#define LNX_VER     LNX_MK_VER(2,6,20)
#define LNX_SUFFIX  RT_CONCAT(_2_6_20,LNX_BIT_SUFFIX)
#include LNX_TEMPLATE_HEADER

#define LNX_VER     LNX_MK_VER(2,6,18)
#define LNX_SUFFIX  RT_CONCAT(_2_6_18,LNX_BIT_SUFFIX)
#include LNX_TEMPLATE_HEADER

#define LNX_VER     LNX_MK_VER(2,6,17)
#define LNX_SUFFIX  RT_CONCAT(_2_6_17,LNX_BIT_SUFFIX)
#include LNX_TEMPLATE_HEADER

#define LNX_VER     LNX_MK_VER(2,6,13)
#define LNX_SUFFIX  RT_CONCAT(_2_6_13,LNX_BIT_SUFFIX)
#include LNX_TEMPLATE_HEADER

#define LNX_VER     LNX_MK_VER(2,6,11)
#define LNX_SUFFIX  RT_CONCAT(_2_6_11,LNX_BIT_SUFFIX)
#include LNX_TEMPLATE_HEADER

#define LNX_VER     LNX_MK_VER(2,6,7)
#define LNX_SUFFIX  RT_CONCAT(_2_6_7,LNX_BIT_SUFFIX)
#include LNX_TEMPLATE_HEADER

#define LNX_VER     LNX_MK_VER(2,5,67)        /* Makes away with kernel_symbol_group and exception_table. */
#define LNX_SUFFIX  RT_CONCAT(_2_5_67,LNX_BIT_SUFFIX)
#include LNX_TEMPLATE_HEADER

#define LNX_VER     LNX_MK_VER(2,5,55)        /* Adds gpl_symbols */
#define LNX_SUFFIX  RT_CONCAT(_2_5_55,LNX_BIT_SUFFIX)
#include LNX_TEMPLATE_HEADER

#define LNX_VER     LNX_MK_VER(2,5,48)
#define LNX_SUFFIX  RT_CONCAT(_2_5_48,LNX_BIT_SUFFIX)
#include LNX_TEMPLATE_HEADER


/*
 * Cleanup.
 */
#undef LNX_PTR_T
#undef LNX_64BIT
#undef LNX_BIT_SUFFIX

