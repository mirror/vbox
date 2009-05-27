/* $Id$ */
/** @file
 * IPRT - RTDirCreateTemp, generic implementation.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/dir.h>

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/rand.h>
#include <iprt/string.h>


RTDECL(int) RTDirCreateTemp(char *pszTemplate)
{
    /*
     * Validate input.
     */
    AssertPtr(pszTemplate);
    unsigned    cXes = 0;
    char       *pszX = strchr(pszTemplate, '\0');
    while (pszX != pszTemplate && pszX[-1] == 'X')
    {
        pszX--;
        cXes++;
    }
    if (!cXes)
    {
        AssertFailed();
        *pszTemplate = '\0';
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Try ten thousand times.
     */
    int i = 10000;
    while (i-- > 0)
    {
        static char const s_sz[] = "0123456789abcdefghijklmnopqrstuvwxyz";
        unsigned j = cXes;
        while (j-- > 0)
            pszX[j] = s_sz[RTRandU32Ex(0, RT_ELEMENTS(s_sz) - 1)];
        int rc = RTDirCreate(pszTemplate, 0700);
        if (RT_SUCCESS(rc))
            return rc;
        if (rc != VERR_ALREADY_EXISTS)
        {
            *pszTemplate = '\0';
            return rc;
        }
    }

    /* we've given up. */
    *pszTemplate = '\0';
    return VERR_ALREADY_EXISTS;
}

