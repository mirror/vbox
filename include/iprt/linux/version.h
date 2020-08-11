/* $Id$ */
/** @file
 * IPRT - Linux kernel version.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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

#ifndef IPRT_INCLUDED_linux_version_h
#define IPRT_INCLUDED_linux_version_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <linux/version.h>

/** @def RTLNX_VER_MIN
 * Evaluates to true if the linux kernel version is equal or higher to the
 * one specfied. */
#define RTLNX_VER_MIN(a_Major, a_Minor, a_Patch) \
    (LINUX_VERSION_CODE >= KERNEL_VERSION(a_Major, a_Minor, a_Patch))

/** @def RTLNX_VER_MAX
 * Evaluates to true if the linux kernel version is less to the one specfied
 * (exclusive). */
#define RTLNX_VER_MAX(a_Major, a_Minor, a_Patch) \
    (LINUX_VERSION_CODE < KERNEL_VERSION(a_Major, a_Minor, a_Patch))

/** @def RTLNX_VER_RANGE
 * Evaluates to true if the linux kernel version is equal or higher to the given
 * minimum version and less (but not equal) to the maximum version (exclusive). */
#define RTLNX_VER_RANGE(a_MajorMin, a_MinorMin, a_PatchMin,  a_MajorMax, a_MinorMax, a_PatchMax) \
    (   LINUX_VERSION_CODE >= KERNEL_VERSION(a_MajorMin, a_MinorMin, a_PatchMin) \
     && LINUX_VERSION_CODE <  KERNEL_VERSION(a_MajorMax, a_MinorMax, a_PatchMax) )


/** @def RTLNX_RHEL_PREREQ
 * Require a minium RedHat release.
 * @param a_iMajor      The major release number (RHEL_MAJOR).
 * @param a_iMinor      The minor release number (RHEL_MINOR).
 * @sa RTLNX_RHEL_MAJ_PREREQ
 */
#if defined(RHEL_MAJOR) && defined(RHEL_MINOR)
# define RTLNX_RHEL_PREREQ(a_iMajor, a_iMinor) \
     ((RHEL_MAJOR) > (a_iMajor) || ((RHEL_MAJOR) == (a_iMajor) && (RHEL_MINOR) >= (a_iMinor)))
#else
# define RTLNX_RHEL_PREREQ(a_iMajor, a_iMinor) (0)
#endif

/** @def RTLNX_RHEL_MAJ_PREREQ
 * Require a minimum minor release number for the given RedHat release.
 * @param a_iMajor      RHEL_MAJOR must _equal_ this.
 * @param a_iMinor      RHEL_MINOR must be greater or equal to this.
 * @sa RTLNX_RHEL_PREREQ
 */
#if defined(RHEL_MAJOR) && defined(RHEL_MINOR)
# define RTLNX_RHEL_MAJ_PREREQ(a_iMajor, a_iMinor) ((RHEL_MAJOR) == (a_iMajor) && (RHEL_MINOR) >= (a_iMinor))
#else
# define RTLNX_RHEL_MAJ_PREREQ(a_iMajor, a_iMinor) (0)
#endif

/** @def RTLNX_SUSE_MAJ_PREREQ
 * Require a minimum minor release number for the given SUSE release.
 * @param a_iMajor      CONFIG_SUSE_VERSION must _equal_ this.
 * @param a_iMinor      CONFIG_SUSE_PATCHLEVEL must be greater or equal to this.
 */
#if defined(CONFIG_SUSE_VERSION) && defined(CONFIG_SUSE_PATCHLEVEL)
# define RTLNX_SUSE_MAJ_PREREQ(a_iMajor, a_iMinor) ((CONFIG_SUSE_VERSION) == (a_iMajor) && (CONFIG_SUSE_PATCHLEVEL) >= (a_iMinor))
#else
# define RTLNX_SUSE_MAJ_PREREQ(a_iMajor, a_iMinor) (0)
#endif


#endif /* !IPRT_INCLUDED_linux_version_h */

