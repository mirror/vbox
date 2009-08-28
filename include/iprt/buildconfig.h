/** @file
 * IPRT - Build Configuration Information
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

#ifndef ___iprt_buildconfig_h
#define ___iprt_buildconfig_h

#include <iprt/cdefs.h>
#include <iprt/types.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_rt_buildconfig    RTBldCfg - Build Configuration Information
 * @ingroup grp_rt
 * @{
 */

/**
 * Gets the source code management revision of the IPRT build.
 * @returns Source code management revision number.
 */
RTDECL(uint32_t)     RTBldCfgRevision(void);

/**
 * Gets the source code management revision of the IPRT build.
 * @returns Read only string containing the revision number.
 */
RTDECL(const char *) RTBldCfgRevisionStr(void);

/**
 * Gets the product version string.
 *
 * This will be a string on the form "x.y.z[_string]".
 *
 * @returns Read only version string.
 *
 * @remarks This is a build time configuration thing that the product using IPRT
 *          will set.  It is therefore not any IPRT version, but rather the
 *          version of that product.
 */
RTDECL(const char *) RTBldCfgVersion(void);

/**
 * Gets the major product version number.
 * @returns Major product version number.
 * @remarks See RTBldCfgVersion.
 */
RTDECL(uint32_t)     RTBldCfgVersionMajor(void);

/**
 * Gets the minor product version number.
 * @returns Minor product version number.
 * @remarks See RTBldCfgVersion.
 */
RTDECL(uint32_t)     RTBldCfgVersionMinor(void);

/**
 * Gets the product build number.
 * @returns Product build number.
 * @remarks See RTBldCfgVersion.
 */
RTDECL(uint32_t)     RTBldCfgVersionBuild(void);


/** @} */

RT_C_DECLS_END

#endif

