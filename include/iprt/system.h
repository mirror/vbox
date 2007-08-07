/** @file
 * innotek Portable Runtime - System.
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

#ifndef ___iprt_system_h
#define ___iprt_system_h

#include <iprt/cdefs.h>
#include <iprt/types.h>


__BEGIN_DECLS

/** @defgroup grp_rt_system RTSystem - System Information
 * @ingroup grp_rt
 * @{
 */

/**
 * Gets the number of logical (not physical) processors in the system.
 *
 * @returns Number of logical processors in the system.
 */
RTDECL(unsigned) RTSystemProcessorGetCount(void);

/**
 * Gets the active logical processor mask.
 *
 * @returns Active logical processor mask. (bit 0 == logical cpu 0)
 */
RTDECL(uint64_t) RTSystemProcessorGetActiveMask(void);


/** @} */

__END_DECLS

#endif

