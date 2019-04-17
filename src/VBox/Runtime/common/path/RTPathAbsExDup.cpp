/* $Id$ */
/** @file
 * IPRT - RTPathAbsExDup
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
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
#include "internal/iprt.h"
#include <iprt/path.h>
#include <iprt/errcore.h>
#include <iprt/param.h>
#include <iprt/string.h>



RTDECL(char *) RTPathAbsExDup(const char *pszBase, const char *pszPath, uint32_t fFlags)
{
    unsigned    cTries    = 16;
    size_t      cbAbsPath = RTPATH_MAX / 2;
    for (;;)
    {
        char  *pszAbsPath = RTStrAlloc(cbAbsPath);
        if (pszAbsPath)
        {
            size_t cbActual = cbAbsPath;
            int rc = RTPathAbsEx(pszBase, pszPath, fFlags, pszAbsPath, &cbActual);
            if (RT_SUCCESS(rc))
            {
                if (cbActual < cbAbsPath / 2)
                    RTStrRealloc(&pszAbsPath, cbActual + 1);
                return pszAbsPath;
            }

            RTStrFree(pszAbsPath);

            if (rc != VERR_BUFFER_OVERFLOW)
                break;

            if (--cTries == 0)
                break;

            cbAbsPath = RT_MAX(RT_ALIGN_Z(cbActual + 16, 64), cbAbsPath + 256);
        }
        else
            break;
    }
    return NULL;
}

