/* $Id$ */
/** @file
 * innotek Portable Runtime - CRT Strings, strlen().
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/string.h>


/**
 * Find the length of a zeroterminated byte string.
 *
 * @returns String length in bytes.
 * @param   pszString   Zero terminated string.
 */
#ifdef _MSC_VER
# if _MSC_VER >= 1400
__checkReturn size_t  __cdecl strlen(__in_z  const char *pszString)
# else
size_t strlen(const char *pszString)
# endif
#else
size_t strlen(const char *pszString)
#endif
{
    register const char *psz = pszString;
    while (*psz)
        psz++;
    return psz - pszString;
}

