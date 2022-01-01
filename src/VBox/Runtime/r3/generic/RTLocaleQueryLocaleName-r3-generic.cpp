/* $Id$ */
/** @file
 * IPRT - RTLocaleQueryLocaleName, ring-3 generic.
 */

/*
 * Copyright (C) 2017-2022 Oracle Corporation
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
#include <locale.h>

#include <iprt/locale.h>
#include "internal/iprt.h"

#include <iprt/errcore.h>
#include <iprt/string.h>
#ifdef RT_OS_SOLARIS
#include <iprt/path.h>
#endif /* RT_OS_SOLARIS */



RTDECL(int) RTLocaleQueryLocaleName(char *pszName, size_t cbName)
{
    const char *pszLocale = setlocale(LC_ALL, NULL);
    if (pszLocale)
    {
#ifdef RT_OS_SOLARIS /* Solaris can return a locale starting with a slash ('/'), e.g. /en_GB.UTF-8/C/C/C/C/C */
        if (RTPATH_IS_SLASH(*pszLocale))
            pszLocale++;
#endif /* RT_OS_SOLARIS */
        return RTStrCopy(pszName, cbName, pszLocale);
    }
    return VERR_NOT_AVAILABLE;
}

