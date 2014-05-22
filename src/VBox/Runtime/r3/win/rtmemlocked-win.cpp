/* $Id$ */
/** @file
 * IPRT - RTMemLocked*, POSIX.
 */

/*
 * Copyright (C) 2014 Oracle Corporation
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
#define LOG_GROUP RTLOGGROUP_MEM
#include "internal/iprt.h"
#include <iprt/mem.h>

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/param.h>
#include <iprt/string.h>
#include "internal/mem.h"

#include <Windows.h>

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

RTDECL(int) RTMemLockedAllocExTag(size_t cb, const char *pszTag, void **ppv) RT_NO_THROW
{
    int rc = VINF_SUCCESS;

    *ppv = RTMemAlloc(cb);
    if (!*ppv)
        rc = VERR_NO_MEMORY;

    return rc;
}

RTDECL(int) RTMemLockedAllocZExTag(size_t cb, const char *pszTag, void **ppv) RT_NO_THROW
{
    void *pv = NULL;
    int rc = RTMemLockedAllocExTag(cb, pszTag, &pv);

    if (RT_SUCCESS(rc))
    {
        RT_BZERO(pv, cb);
        *ppv = pv;
    }

    return rc;
}

RTDECL(void *) RTMemLockedAllocTag(size_t cb, const char *pszTag) RT_NO_THROW
{
    void *pv = NULL;
    int rc = RTMemLockedAllocExTag(cb, pszTag, &pv);

    if (RT_FAILURE(rc))
        return NULL;

    return pv;
}

RTDECL(void *) RTMemLockedAllocZTag(size_t cb, const char *pszTag) RT_NO_THROW
{
    void *pv = NULL;
    int rc = RTMemLockedAllocZExTag(cb, pszTag, &pv);

    if (RT_FAILURE(rc))
        return NULL;

    return pv;
}

RTDECL(void) RTMemLockedFree(void *pv) RT_NO_THROW
{
    /*
     * Validate & adjust the input.
     */
    if (!pv)
        return;
    AssertPtr(pv);

    RTMemFree(pv);
}

