/** @file
 * IPRT - System Information.
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
 * Info level for RTSystemGetOSInfo().
 */
typedef enum RTSYSOSINFO
{
    RTSYSOSINFO_INVALID = 0,    /**< The usual invalid entry. */
    RTSYSOSINFO_PRODUCT,        /**< OS product name. (uname -o) */
    RTSYSOSINFO_RELEASE,        /**< OS release. (uname -r) */
    RTSYSOSINFO_VERSION,        /**< OS version, optional. (uname -v) */
    RTSYSOSINFO_SERVICE_PACK,   /**< Service/fix pack level, optional. */
    RTSYSOSINFO_END             /**< End of the valid info levels. */
} RTSYSOSINFO;


/**
 * Queries information about the OS.
 *
 * @returns IPRT status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_PARAMETER if enmInfo is invalid.
 * @retval  VERR_INVALID_POINTER if pszInfoStr is invalid.
 * @retval  VERR_BUFFER_OVERFLOW if the buffer is too small. The buffer will
 *          contain the chopped off result in this case, provided cchInfo isn't 0.
 * @retval  VERR_NOT_SUPPORTED if the info level isn't implemented. The buffer will
 *          contain an empty string.
 *
 * @param   enmInfo         The OS info level.
 * @param   pszInfo         Where to store the result.
 * @param   cchInfo         The size of the output buffer.
 */
RTDECL(int) RTSystemQueryOSInfo(RTSYSOSINFO enmInfo, char *pszInfo, size_t cchInfo);


/** @} */

__END_DECLS

#endif

