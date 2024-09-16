/* $Id$ */
/** @file
 * IPRT - UTF-16 Locale Specific Manipulation, Win32.
 */

/*
 * Copyright (C) 2006-2024 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP RTLOGGROUP_UTF16
#include <iprt/win/windows.h>

#include <iprt/utf16.h>


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

