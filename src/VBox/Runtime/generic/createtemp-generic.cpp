/* $Id$ */
/** @file
 * IPRT - RTDirCreateTemp, generic implementation.
 */

/*
 * Copyright (C) 2009 Oracle Corporation
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/dir.h>
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/path.h>
#include <iprt/rand.h>
#include <iprt/string.h>


static int rtCreateTempValidateTemplate(char *pszTemplate, char **ppszX,
                                        unsigned *pcXes)
{
    /*
     * Validate input and count X'es.
     *
     * The X'es may be trailing, or they may be a cluster of 3 or more inside
     * the file name.
     */
    AssertPtr(pszTemplate);
    AssertPtr(ppszX);
    AssertPtr(pcXes);
    unsigned    cXes = 0;
    char       *pszX = strchr(pszTemplate, '\0');
    if (   pszX != pszTemplate
        && pszX[-1] != 'X')
    {
        /* look inside the file name. */
        char *pszFilename = RTPathFilename(pszTemplate);
        if (   pszFilename
            && (size_t)(pszX - pszFilename) > 3)
        {
            char *pszXEnd = pszX - 1;
            pszFilename += 3;
            do
            {
                if (    pszXEnd[-1] == 'X'
                    &&  pszXEnd[-2] == 'X'
                    &&  pszXEnd[-3] == 'X')
                {
                    pszX = pszXEnd - 3;
                    cXes = 3;
                    break;
                }
            } while (pszXEnd-- != pszFilename);
        }
    }

    /* count them */
    while (   pszX != pszTemplate
           && pszX[-1] == 'X')
    {
        pszX--;
        cXes++;
    }

    /* fail if none found. */
    if (!cXes)
    {
        AssertFailed();
        return VERR_INVALID_PARAMETER;
    }
    *ppszX = pszX;
    *pcXes = cXes;
    return VINF_SUCCESS;
}


static void rtCreateTempFillTemplate(char *pszX, unsigned cXes)
{
    static char const s_sz[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    unsigned j = cXes;
    while (j-- > 0)
        pszX[j] = s_sz[RTRandU32Ex(0, RT_ELEMENTS(s_sz) - 2)];
}


RTDECL(int) RTDirCreateTemp(char *pszTemplate, RTFMODE fMode)
{
    char       *pszX;
    unsigned    cXes;
    int rc = rtCreateTempValidateTemplate(pszTemplate, &pszX, &cXes);
    if (RT_FAILURE(rc))
    {
        *pszTemplate = '\0';
        return rc;
    }
    /*
     * Try ten thousand times.
     */
    int i = 10000;
    while (i-- > 0)
    {
        rtCreateTempFillTemplate(pszX, cXes);
        rc = RTDirCreate(pszTemplate, fMode, 0);
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
RT_EXPORT_SYMBOL(RTDirCreateTemp);

