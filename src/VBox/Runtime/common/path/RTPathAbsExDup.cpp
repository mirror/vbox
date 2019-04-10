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
    char szPath[RTPATH_MAX];
    size_t cbPath = sizeof(szPath);
    int rc = RTPathAbsEx(pszBase, pszPath, fFlags, szPath, &cbPath);
    if (RT_SUCCESS(rc))
        return RTStrDup(szPath);

    if (rc == VERR_BUFFER_OVERFLOW)
    {
        size_t   cbPrevPath = sizeof(szPath);
        uint32_t cTries = 8;
        while (cTries-- > 0)
        {
            cbPath     = RT_MAX(RT_ALIGN_Z(cbPath + 16, 64), cbPrevPath + 256);
            cbPrevPath = cbPath;
            char *pszAbsPath = (char *)RTStrAlloc(cbPath);
            if (pszAbsPath)
            {
                rc = RTPathAbsEx(pszBase, pszPath, fFlags, pszAbsPath, &cbPath);
                if (RT_SUCCESS(rc))
                    return pszAbsPath;
                RTStrFree(pszAbsPath);
            }
            else
                break;
        }
    }
    return NULL;
}

