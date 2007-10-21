/* $Id$ */
/** @file
 * innotek Portable Runtime - UTF-16 Locale Specific Manipulation, Win32.
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
#define LOG_GROUP RTLOGGROUP_UTF16
#include <Windows.h>

#include <iprt/string.h>


RTDECL(int) RTUtf16LocaleICmp(PCRTUTF16 pusz1, PCRTUTF16 pusz2)
{
    if (pusz1 == pusz2)
        return 0;
    if (pusz1 == NULL)
        return -1;
    if (pusz2 == NULL)
        return 1;

    return CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE, pusz1, -1, pusz2, -1) - 2;
}

