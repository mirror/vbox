/* $Id */
/** @file
 * tstVBoxGINA.cpp - Simple testcase for invoking VBoxGINA.dll.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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

#define UNICODE
#include <windows.h>
#include <stdio.h>

int main(int argc, TCHAR* argv[])
{
    DWORD dwErr;

    /**
     * Be sure that:
     * - the debug VBoxGINA gets loaded instead of a maybe installed
     *   release version in "C:\Windows\system32".
     */

    HMODULE hMod = LoadLibrary(L"VBoxGINA.dll");
    if (!hMod)
    {
        dwErr = GetLastError();
        wprintf(L"VBoxGINA.dll not found, error=%ld\n", dwErr);
    }
    else
    {
        wprintf(L"VBoxGINA found\n");

        FARPROC pfnDebug = GetProcAddress(hMod, "VBoxGINADebug");
        if (!pfnDebug)
        {
            dwErr = GetLastError();
            wprintf(L"Could not load VBoxGINADebug, error=%ld\n", dwErr);
        }
        else
        {
            wprintf(L"Calling VBoxGINA ...\n");
            dwErr = pfnDebug();
        }

        FreeLibrary(hMod);
    }

    wprintf(L"Test returned: %ld\n", dwErr);

    return dwErr == ERROR_SUCCESS ? 0 : 1;
}
