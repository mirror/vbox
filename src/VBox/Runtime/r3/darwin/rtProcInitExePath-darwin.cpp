/* $Id$ */
/** @file
 * IPRT - rtProcInitName, Darwin.
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP RTLOGGROUP_PROCESS
#ifdef RT_OS_DARWIN
# include <mach-o/dyld.h>
#endif

#include <iprt/string.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/path.h>
#include "internal/process.h"
#include "internal/path.h"


DECLHIDDEN(int) rtProcInitExePath(char *pszPath, size_t cchPath)
{
    /*
     * Query the image name from the dynamic linker, convert and return it.
     */
    const char *pszImageName = _dyld_get_image_name(0);
    AssertReturn(pszImageName, VERR_INTERNAL_ERROR);

    char *pszTmp;
    int rc = rtPathFromNative(&pszTmp, pszImageName);
    AssertMsgRCReturn(rc, ("rc=%Rrc pszLink=\"%s\"\nhex: %.*Rhsx\n", rc, pszPath, strlen(pszImageName), pszPath), rc);

    size_t cch = strlen(pszTmp);
    AssertReturn(cch <= cchPath, VERR_BUFFER_OVERFLOW);

    memcpy(pszPath, pszTmp, cch + 1);
    RTStrFree(pszTmp);

    return VINF_SUCCESS;
}

