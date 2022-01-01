/* $Id$ */
/** @file
 * IPRT - RTUtf16NCmpUtf8.
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
#include <iprt/utf16.h>
#include "internal/iprt.h"


RTDECL(int) RTUtf16NCmpUtf8(PCRTUTF16 pwsz1, const char *psz2, size_t cwcMax1, size_t cchMax2)
{
    if (!pwsz1 || !cwcMax1)
        return -1;
    if (!psz2 || !cchMax2)
        return 1;

    while (cwcMax1 > 0 && cchMax2 > 0)
    {
        RTUNICP uc1;
        int rc = RTUtf16GetCpNEx(&pwsz1, &cwcMax1, &uc1);
        if (RT_SUCCESS(rc))
        {
            RTUNICP uc2;
            rc = RTStrGetCpNEx(&psz2, &cchMax2, &uc2);
            if (RT_SUCCESS(rc))
            {
                if (uc1 != uc2)
                    return uc1 < uc2 ? -1 : 1;
                if (!uc1)
                    return 0;
            }
            else
                return 1;
        }
    }
    return 0;
}
RT_EXPORT_SYMBOL(RTUtf16NCmpUtf8);

