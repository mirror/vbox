/* $Id$ */
/** @file
 * IPRT - Environment, RTEnvDupEx, generic.
 */

/*
 * Copyright (C) 2010-2022 Oracle Corporation
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
#include <iprt/env.h>
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/string.h>



RTDECL(char *) RTEnvDupEx(RTENV Env, const char *pszVar)
{
    /*
     * Try with a small buffer.  This helps avoid allocating a heap buffer for
     * variables that doesn't exist.
     */
    char szSmall[256];
    int rc = RTEnvGetEx(Env, pszVar, szSmall, sizeof(szSmall), NULL);
    if (RT_SUCCESS(rc))
        return RTStrDup(szSmall);
    if (rc != VERR_BUFFER_OVERFLOW)
        return NULL;

    /*
     * It's a big bugger.
     */
    size_t cbBuf = _1K;
    do
    {
        char *pszBuf = RTStrAlloc(cbBuf);
        AssertBreak(pszBuf);

        rc = RTEnvGetEx(Env, pszVar, pszBuf, cbBuf, NULL);
        if (RT_SUCCESS(rc))
            return pszBuf;

        RTStrFree(pszBuf);

        /* If overflow double the buffer. */
        if (rc != VERR_BUFFER_OVERFLOW)
            break;
        cbBuf *= 2;
    } while (cbBuf < _64M);

    return NULL;
}
RT_EXPORT_SYMBOL(RTEnvDupEx);

