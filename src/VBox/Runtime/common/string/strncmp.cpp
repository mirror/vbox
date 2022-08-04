/* $Id$ */
/** @file
 * IPRT - CRT Strings, strncmp().
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

#ifdef IPRT_NO_CRT
# undef strncmp
int RT_NOCRT(strncmp)(const char *pszStr1, const char *pszStr2, size_t cb)
#elif defined( _MSC_VER)
_CRTIMP int __cdecl strncmp(const char *pszStr1, const char *pszStr2, size_t cb)
#elif defined(__WATCOMC__) && !defined(IPRT_NO_CRT)
_WCRTLINK int std::strncmp(const char *pszStr1, const char *pszStr2, size_t cb)
#else
int strncmp(const char *pszStr1, const char *pszStr2, size_t cb)
# if defined(__THROW) && !defined(RT_OS_WINDOWS) && !defined(RT_OS_OS2)
    __THROW
# endif
#endif
{
    while (cb-- > 0)
    {
        char const ch1   = *pszStr1++;
        int  const iDiff = ch1 - *pszStr2++;
        if (iDiff)
            return iDiff;
        if (!ch1)
            break;
    }
    return 0;
}
RT_ALIAS_AND_EXPORT_NOCRT_SYMBOL(strncmp);

