/* $Id$ */
/** @file
 * IPRT - CRT Strings, strstr().
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
 * Find the location of @a pszSubStr in @a pszString.
 *
 * @returns Pointer to first occurence of the substring in @a pszString, NULL if
 *          not found.
 * @param   pszString   Zero terminated string to search.
 * @param   pszSubStr   The substring to search for.
 */
#undef strstr
char *RT_NOCRT(strstr)(const char *pszString, const char *pszSubStr)
{
    char const  ch0Sub = *pszSubStr;
    pszString = strchr(pszString, ch0Sub);
    if (pszString)
    {
        size_t const cchSubStr = strlen(pszSubStr);
        do
        {
            if (strncmp(pszString, pszSubStr, cchSubStr) == 0)
                return (char *)pszString;
            if (ch0Sub)
                pszString = strchr(pszString + 1, ch0Sub);
            else
                break;
        } while (pszString != NULL);
    }
    return NULL;
}
RT_ALIAS_AND_EXPORT_NOCRT_SYMBOL(strstr);

