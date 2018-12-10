/* $Id$ */
/** @file
 * IPRT - RTPathFilenameUtf16
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
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

#include <iprt/assert.h>



RTDECL(PRTUTF16) RTPathFilenameUtf16(PCRTUTF16 pwszPath)
{
    return RTPathFilenameExUtf16(pwszPath, RTPATH_STYLE);
}
RT_EXPORT_SYMBOL(RTPathFilenameUtf16);


RTDECL(PRTUTF16) RTPathFilenameExUtf16(PCRTUTF16 pwszPath, uint32_t fFlags)
{
    PCRTUTF16 pwsz = pwszPath;
    PCRTUTF16 pwszName = pwszPath;

    Assert(RTPATH_STR_F_IS_VALID(fFlags, 0 /*no extra flags*/));
    fFlags &= RTPATH_STR_F_STYLE_MASK;
    if (fFlags == RTPATH_STR_F_STYLE_HOST)
        fFlags = RTPATH_STYLE;
    if (fFlags == RTPATH_STR_F_STYLE_DOS)
    {
        for (;; pwsz++)
        {
            switch (*pwsz)
            {
                /* handle separators. */
                case ':':
                case '\\':
                case '/':
                    pwszName = pwsz + 1;
                    break;

                /* the end */
                case '\0':
                    if (*pwszName)
                        return (PRTUTF16)(void *)pwszName;
                    return NULL;
            }
        }
    }
    else
    {
        Assert(fFlags == RTPATH_STR_F_STYLE_UNIX);
        for (;; pwsz++)
        {
            switch (*pwsz)
            {
                /* handle separators. */
                case '/':
                    pwszName = pwsz + 1;
                    break;

                /* the end */
                case '\0':
                    if (*pwszName)
                        return (PRTUTF16)(void *)pwszName;
                    return NULL;
            }
        }
    }

    /* not reached */
}
RT_EXPORT_SYMBOL(RTPathFilenameExUtf16);

