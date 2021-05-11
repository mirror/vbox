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

/* We need utsrelease.h in order to detect Ubuntu kernel,
 * i.e. check if UTS_UBUNTU_RELEASE_ABI is defined. Support kernels
 * starting from Ubuntu 14.04 Trusty which is based on upstream
 * kernel 3.13.11. */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,13,11))
# include <generated/utsrelease.h>
# include <iprt/cdefs.h>
#endif

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


/** @def RTLNX_RHEL_MIN
 * Require a minium RedHat release.
 * @param a_iMajor      The major release number (RHEL_MAJOR).
 * @param a_iMinor      The minor release number (RHEL_MINOR).
 * @sa RTLNX_RHEL_MAX, RTLNX_RHEL_RANGE, RTLNX_RHEL_MAJ_PREREQ
 */
#if defined(RHEL_MAJOR) && defined(RHEL_MINOR)
# define RTLNX_RHEL_MIN(a_iMajor, a_iMinor) \
     ((RHEL_MAJOR) > (a_iMajor) || ((RHEL_MAJOR) == (a_iMajor) && (RHEL_MINOR) >= (a_iMinor)))
#else
# define RTLNX_RHEL_MIN(a_iMajor, a_iMinor) (0)
#endif

/** @def RTLNX_RHEL_MAX
 * Require a maximum RedHat release, true for all RHEL versions below it.
 * @param a_iMajor      The major release number (RHEL_MAJOR).
 * @param a_iMinor      The minor release number (RHEL_MINOR).
 * @sa RTLNX_RHEL_MIN, RTLNX_RHEL_RANGE, RTLNX_RHEL_MAJ_PREREQ
 */
#if defined(RHEL_MAJOR) && defined(RHEL_MINOR)
# define RTLNX_RHEL_MAX(a_iMajor, a_iMinor) \
     ((RHEL_MAJOR) < (a_iMajor) || ((RHEL_MAJOR) == (a_iMajor) && (RHEL_MINOR) < (a_iMinor)))
#else
# define RTLNX_RHEL_MAX(a_iMajor, a_iMinor) (0)
#endif

/** @def RTLNX_RHEL_RANGE
 * Check that it's a RedHat kernel in the given version range.
 * The max version is exclusive, the minimum inclusive.
 * @sa RTLNX_RHEL_MIN, RTLNX_RHEL_MAX, RTLNX_RHEL_MAJ_PREREQ
 */
#if defined(RHEL_MAJOR) && defined(RHEL_MINOR)
# define RTLNX_RHEL_RANGE(a_iMajorMin, a_iMinorMin,  a_iMajorMax, a_iMinorMax) \
     (RTLNX_RHEL_MIN(a_iMajorMin, a_iMinorMin) && RTLNX_RHEL_MAX(a_iMajorMax, a_iMinorMax))
#else
# define RTLNX_RHEL_RANGE(a_iMajorMin, a_iMinorMin,  a_iMajorMax, a_iMinorMax)  (0)
#endif

/** @def RTLNX_RHEL_MAJ_PREREQ
 * Require a minimum minor release number for the given RedHat release.
 * @param a_iMajor      RHEL_MAJOR must _equal_ this.
 * @param a_iMinor      RHEL_MINOR must be greater or equal to this.
 * @sa RTLNX_RHEL_MIN, RTLNX_RHEL_MAX
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


#if defined(UTS_UBUNTU_RELEASE_ABI)
/** @def RTLNX_UBUNTU_VERSION
 * Ubuntu kernels are based on upstream kernels and provide
 * version information with regular KERNEL_VERSION() macro. However,
 * Ubuntu kernel additionally provides UTS_UBUNTU_RELEASE_ABI macro
 * which increases with the kernel update while KERNEL_VERSION() info
 * stays the same.
 *
 * UTS_UBUNTU_RELEASE_ABI does not increase constantly, but rather reset
 * when the next kernel update is based on a newer upstream version (i.e.
 * KERNEL_VERSION() value has changed). Therefore, in order to compare
 * Ubuntu kernels, UTS_UBUNTU_RELEASE_ABI macro is treated as a KERNEL_VERSION()
 * extension and appended to the end of it. So, regular 24-bit-value
 * of KERNEL_VERSION() becomes 32-bit-value, as can be seen below.
 *
 * NOTE: @a_iVer is expected to be in range of [0x000000..0xFFFFFF],
 * @a_iAbi is expected to be in range of [0x00..0xFF].
 *
 * @param a_iVer        The 24-bit-value of kernel version number, result
 *                      of KERNEL_VERSION() macro.
 * @param a_iAbi        8-bit Ubuntu kernel ABI version number.
 */
# define RTLNX_UBUNTU_VERSION(a_iVer, a_iAbi) \
    (((a_iVer) << 8) | (RT_MIN((a_iAbi), 0xFF)))

/** @def RTLNX_UBUNTU_MIN
 * Require Ubuntu release to be equal or newer than specified version.
 * @param a_iMajor      The major kernel version number.
 * @param a_iMinor      The minor kernel version number.
 * @param a_iPatch      The micro kernel version number.
 * @param a_iAbi        Ubuntu kernel ABI version number.
 */
# define RTLNX_UBUNTU_MIN(a_iMajor, a_iMinor, a_iPatch, a_iAbi) \
    (   RTLNX_UBUNTU_VERSION(LINUX_VERSION_CODE, UTS_UBUNTU_RELEASE_ABI) \
     >= RTLNX_UBUNTU_VERSION(KERNEL_VERSION(a_iMajor, a_iMinor, a_iPatch), a_iAbi) )

/** @def RTLNX_UBUNTU_MAX
 * Require Ubuntu release to be older than specified version.
 * @param a_iMajor      The major kernel version number.
 * @param a_iMinor      The minor kernel version number.
 * @param a_iPatch      The micro kernel version number.
 * @param a_iAbi        Ubuntu kernel ABI version number.
 */
# define RTLNX_UBUNTU_MAX(a_iMajor, a_iMinor, a_iPatch, a_iAbi) \
    (   RTLNX_UBUNTU_VERSION(LINUX_VERSION_CODE, UTS_UBUNTU_RELEASE_ABI) \
     <  RTLNX_UBUNTU_VERSION(KERNEL_VERSION(a_iMajor, a_iMinor, a_iPatch), a_iAbi) )
#else /* !UTS_UBUNTU_RELEASE_ABI */
# define RTLNX_UBUNTU_MIN(a_iMajor, a_iMinor, a_iPatch, a_iAbi)  (0)
# define RTLNX_UBUNTU_MAX(a_iMajor, a_iMinor, a_iPatch, a_iAbi)  (0)
#endif /* !UTS_UBUNTU_RELEASE_ABI */

#endif /* !IPRT_INCLUDED_linux_version_h */

