/* $Id$ */
/** @file
 * IPRT - RTPathFindCommon implementations.
 */

/*
 * Copyright (C) 2020 Oracle Corporation
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
#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/path.h>


RTDECL(size_t) RTPathFindCommonEx(const char * const *papcszPaths, size_t cPaths, char szSeparator)
{
    AssertPtrReturn(papcszPaths, 0);
    AssertReturn(cPaths, 0);
    AssertReturn(szSeparator != '\0', 0);

    const char *pcszRef = papcszPaths[0]; /* The reference we're comparing with. */

    size_t cch = 0;
    do
    {
        for (size_t i = 0; i < cPaths; ++i)
        {
            const char *pcszPath = papcszPaths[i];
            if (   pcszPath
                && pcszPath[cch]
                && pcszPath[cch] == pcszRef[cch])
                continue;

            while (   cch
                   && pcszRef[--cch] != szSeparator) { }

            return cch ? cch + 1 : 0;
        }
    }
    while (++cch);

    return 0;
}
RT_EXPORT_SYMBOL(RTPathFindCommonEx);


RTDECL(size_t) RTPathFindCommon(const char * const *papcszPaths, size_t cPaths)
{
    return RTPathFindCommonEx(papcszPaths, cPaths, RTPATH_SLASH);
}
RT_EXPORT_SYMBOL(RTPathFindCommon);

