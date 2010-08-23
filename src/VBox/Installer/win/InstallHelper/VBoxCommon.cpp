/* $Id: */
/** @file
* VBoxCommon - Misc helper routines for install helper.
*/

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <windows.h>
#include <tchar.h>
#include <stdio.h>

#include <msi.h>
#include <msiquery.h>


#if (_MSC_VER < 1400) /* Provide _stprintf_s to VC < 8.0. */
int _stprintf_s(TCHAR *buffer, size_t cbBuffer, const TCHAR *format, ...)
{
    int ret;
    va_list args;
    va_start(args, format);
    ret = _vsntprintf(buffer, cbBuffer, format, args);
    va_end(args);
    return ret;
}
#endif

UINT VBoxGetProperty(MSIHANDLE a_hModule, TCHAR* a_pszName, TCHAR* a_pValue, DWORD a_dwSize)
{
    UINT uiRet = ERROR_SUCCESS;
    DWORD dwBuffer = 0;

    uiRet = MsiGetProperty(a_hModule, a_pszName, TEXT(""), &dwBuffer);
    if (ERROR_MORE_DATA == uiRet)
    {
        ++dwBuffer;     /* On output does not include terminating null, so add 1. */

        if (dwBuffer > a_dwSize)
            return ERROR_MORE_DATA;

        ZeroMemory(a_pValue, a_dwSize);
        uiRet = MsiGetProperty(a_hModule, a_pszName, a_pValue, &dwBuffer);
    }

    return uiRet;
}

UINT VBoxSetProperty(MSIHANDLE a_hModule, TCHAR* a_pszName, TCHAR* a_pValue)
{
    return MsiSetProperty(a_hModule, a_pszName, a_pValue);
}

