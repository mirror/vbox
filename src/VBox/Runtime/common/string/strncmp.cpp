/* $Id$ */
/** @file
 * innotek Portable Runtime - CRT Strings, strncmp().
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <iprt/string.h>

#ifdef _MSC_VER
_CRTIMP int __cdecl strncmp
#else
int strncmp
#endif
    (const char *pszStr1, const char *pszStr2, size_t cb)
#if defined(__THROW) && !defined(RT_OS_WINDOWS) && !defined(RT_OS_OS2)
    __THROW
#endif
{
    const char* fini = pszStr1+cb;
    while (pszStr1 < fini)
    {
        int res=*pszStr1-*pszStr2;
        if (res)
            return res;
        if (!*pszStr1)
            return 0;
        ++pszStr1; ++pszStr2;
    }
    return 0;
}
