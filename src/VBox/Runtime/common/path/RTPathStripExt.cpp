/* $Id$ */
/** @file
 * IPRT - RTPathStripExt
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
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
#include "internal/iprt.h"
#include <iprt/path.h>
#include <iprt/string.h>



/**
 * Strips the extension from a path.
 *
 * @param   pszPath     Path which extension should be stripped.
 */
RTDECL(void) RTPathStripExt(char *pszPath)
{
    char *pszDot = (char *)NULL;
    for (;; pszPath++)
    {
        switch (*pszPath)
        {
            /* handle separators. */
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
            case ':':
            case '\\':
#endif
            case '/':
                pszDot = (char *)NULL;
                break;
            case '.':
                pszDot = pszPath;
                break;

            /* the end */
            case '\0':
                if (pszDot)
                    *pszDot = '\0';
                return;
        }
    }
    /* will never get here */
}

