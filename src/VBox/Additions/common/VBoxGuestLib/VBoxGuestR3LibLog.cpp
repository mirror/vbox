/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, Logging.
 */

/*
 * Copyright (C) 2007-2016 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/mem.h>
#include "VBGLR3Internal.h"


/**
 * Write to the backdoor logger from ring 3 guest code.
 *
 * @returns IPRT status code.
 *
 * @param   pch     The string to log.  Does not need to be terminated.
 * @param   cch     The number of chars (bytes) to log.
 *
 * @remarks This currently does not accept more than 255 bytes of data at
 *          one time. It should probably be rewritten to use pass a pointer
 *          in the IOCtl.
 */
VBGLR3DECL(int) VbglR3WriteLog(const char *pch, size_t cch)
{
    /*
     * Quietly skip NULL strings.
     * (Happens in the RTLogBackdoorPrintf case.)
     */
    if (!cch)
        return VINF_SUCCESS;
    if (!VALID_PTR(pch))
        return VERR_INVALID_POINTER;

#ifdef RT_OS_WINDOWS
    /*
     * Duplicate the string as it may be read only (a C string).
     */
    void *pvTmp = RTMemDup(pch, cch);
    if (!pvTmp)
        return VERR_NO_MEMORY;
    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_LOG(cch), pvTmp, cch);
    RTMemFree(pvTmp);
    return rc;

#elif 0 /** @todo Several OSes could take this route (solaris and freebsd for instance). */
    /*
     * Handle the entire request in one go.
     */
    return vbglR3DoIOCtl(VBOXGUEST_IOCTL_LOG(cch), pvTmp, cch);

#else
    /*
     * *BSD does not accept more than 4KB per ioctl request, while
     * Linux can't express sizes above 8KB, so, split it up into 2KB chunks.
     */
# define STEP 2048
    int rc = VINF_SUCCESS;
    for (size_t off = 0; off < cch && RT_SUCCESS(rc); off += STEP)
    {
        size_t cbStep = RT_MIN(cch - off, STEP);
        rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_LOG(cbStep), (char *)pch + off, cbStep);
    }
# undef STEP
    return rc;
#endif
}

