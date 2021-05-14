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
 * kernel 3.13.x. */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,13,0))
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
/* Ubuntu kernel ABI version is represented as a constant which can be interpreted
 * by compiler as an octal constant (e.g. 050818). We add '0x' prefix to it in
 * order to be able to treat it as a hexadecimal number. */
# define RTLNX_UBUNTU_ABI(a_iAbi)   (RT_CONCAT(0x,a_iAbi))

/** @def RTLNX_UBUNTU_ABI_MIN
 * Require Ubuntu release to be equal or newer than specified version.
 * Kernel version should exactly match to specified @a_iMajor, @a_iMinor
 * and @a_iPatch. Number @a_iAbi should be equal or greater than current
 * ABI version.
 *
 * @param a_iMajor      The major kernel version number.
 * @param a_iMinor      The minor kernel version number.
 * @param a_iPatch      The micro kernel version number.
 * @param a_iAbi        Ubuntu kernel ABI version number.
 */
# define RTLNX_UBUNTU_ABI_MIN(a_iMajor, a_iMinor, a_iPatch, a_iAbi) \
    (   KERNEL_VERSION(a_iMajor, a_iMinor, a_iPatch) == LINUX_VERSION_CODE \
     && RTLNX_UBUNTU_ABI(UTS_UBUNTU_RELEASE_ABI) >= RTLNX_UBUNTU_ABI(a_iAbi))

/** @def RTLNX_UBUNTU_ABI_MAX
 * Require Ubuntu release to be older than specified version.
 * Kernel version should exactly match to specified @a_iMajor, @a_iMinor
 * and @a_iPatch. Number @a_iAbi should be less than current ABI version.
 *
 * @param a_iMajor      The major kernel version number.
 * @param a_iMinor      The minor kernel version number.
 * @param a_iPatch      The micro kernel version number.
 * @param a_iAbi        Ubuntu kernel ABI version number.
 */
# define RTLNX_UBUNTU_ABI_MAX(a_iMajor, a_iMinor, a_iPatch, a_iAbi) \
    (   KERNEL_VERSION(a_iMajor, a_iMinor, a_iPatch) == LINUX_VERSION_CODE \
     && RTLNX_UBUNTU_ABI(UTS_UBUNTU_RELEASE_ABI) < RTLNX_UBUNTU_ABI(a_iAbi))

/** @def RTLNX_UBUNTU_RANGE
 * Require Ubuntu release to be in specified ABI versions range.
 * Kernel version should exactly match to specified @a_iMajor, @a_iMinor
 * and @a_iPatch. Numbers @a_iAbiMin and @a_iAbiMax specify ABI versions range.
 * The max version is exclusive, the minimum inclusive.
 *
 * @param a_iMajor      The major kernel version number.
 * @param a_iMinor      The minor kernel version number.
 * @param a_iPatch      The micro kernel version number.
 * @param a_iAbiMin     Ubuntu kernel ABI version number (minimum).
 * @param a_iAbiMax     Ubuntu kernel ABI version number (maximum).
 */
# define RTLNX_UBUNTU_ABI_RANGE(a_iMajor, a_iMinor, a_iPatch, a_iAbiMin, a_iAbiMax) \
    (   RTLNX_UBUNTU_ABI_MIN(a_iMajor, a_iMinor, a_iPatch, a_iAbiMin) \
     && RTLNX_UBUNTU_ABI_MAX(a_iMajor, a_iMinor, a_iPatch, a_iAbiMax))

#else /* !UTS_UBUNTU_RELEASE_ABI */

# define RTLNX_UBUNTU_ABI_MIN(a_iMajor, a_iMinor, a_iPatch, a_iAbi) (0)
# define RTLNX_UBUNTU_ABI_MAX(a_iMajor, a_iMinor, a_iPatch, a_iAbi) (0)
# define RTLNX_UBUNTU_ABI_RANGE(a_iMajorMin, a_iMinorMin, a_iPatchMin, a_iAbiMin, \
    a_iMajorMax, a_iMinorMax, a_iPatchMax, a_iAbiMax) (0)

#endif /* !UTS_UBUNTU_RELEASE_ABI */

#endif /* !IPRT_INCLUDED_linux_version_h */

