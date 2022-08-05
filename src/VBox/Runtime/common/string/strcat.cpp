/* $Id$ */
/** @file
 * IPRT - CRT Strings, strcat().
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
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
#include <iprt/string.h>


/**
 * Append one string to another.
 *
 * @returns Pointer to destination string
 * @param   pszDst      String to append @a pszSrc to.
 * @param   pszSrc      Zero terminated string.
 */
#undef strcat
char *RT_NOCRT(strcat)(char *pszDst, const char *pszSrc)
{
    char * const pszRet = pszDst;
    pszDst = RTStrEnd(pszDst, RTSTR_MAX);
    while ((*pszDst = *pszSrc++) != '\0')
        pszDst++;

    return pszRet;
}
RT_ALIAS_AND_EXPORT_NOCRT_SYMBOL(strcat);

