/** @file
 * IPRT - C++ path utilities.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
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

#ifndef ___iprt_cpp_path_h
#define ___iprt_cpp_path_h

#include <iprt/types.h>
#include <iprt/assert.h>
#include <iprt/cpp/ministring.h>
#include <iprt/path.h>


/** @defgroup grp_rt_cpp_path    C++ Path Utilities
 * @ingroup grp_rt_cpp
 * @{
 */

/**
 * Wrapper around RTPathAbs() that returns the result in the given buffer.
 *
 * @returns IPRT status code.
 * @param   rStrAbs         Reference to the destiation string.
 * @param   pszRelative     The relative source string.
 */
DECLINLINE(int) RTPathAbsCxx(RTCString &rStrAbs, const char *pszRelative)
{
    Assert(rStrAbs.c_str() != pszRelative);
    int rc = rStrAbs.reserveNoThrow(RTPATH_MAX);
    if (RT_SUCCESS(rc))
    {
        char *pszDst = rStrAbs.mutableRaw();
        rc = RTPathAbs(pszRelative, pszDst, rStrAbs.capacity() - 1);
        if (RT_FAILURE(rc))
            *pszDst = '\0';
        rStrAbs.jolt();
    }
    return rc;
}


/**
 * Wrapper around RTPathAbs() that returns the result in the given buffer.
 *
 * @returns IPRT status code.
 * @param   rStrAbs         Reference to the destiation string.
 * @param   rStrRelative    Reference to the relative source string.
 */
DECLINLINE(int) RTPathAbsCxx(RTCString &rStrAbs, RTCString const &rStrRelative)
{
    return RTPathAbsCxx(rStrAbs, rStrRelative.c_str());
}


/** @} */

#endif

