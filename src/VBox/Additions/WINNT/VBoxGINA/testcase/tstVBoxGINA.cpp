/* $Id$ */
/** @file
 * tstVBoxGINA.cpp - Simple testcase for invoking VBoxGINA.dll.
 */

/*
 * Copyright (C) 2012-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <iprt/win/windows.h>
#include <iprt/stream.h>

int main()
{
    DWORD dwErr;

    /*
     * Be sure that:
     * - the debug VBoxGINA gets loaded instead of a maybe installed
     *   release version in "C:\Windows\system32".
     */

    HMODULE hMod = LoadLibraryW(L"VBoxGINA.dll");
    if (hMod)
    {
        RTPrintf("VBoxGINA found\n");

        FARPROC pfnDebug = GetProcAddress(hMod, "VBoxGINADebug");
        if (pfnDebug)
        {
            RTPrintf("Calling VBoxGINA ...\n");
            dwErr = pfnDebug();
        }
        else
        {
            dwErr = GetLastError();
            RTPrintf("Could not load VBoxGINADebug, error=%u\n", dwErr);
        }

        FreeLibrary(hMod);
    }
    else
    {
        dwErr = GetLastError();
        RTPrintf("VBoxGINA.dll not found, error=%u\n", dwErr);
    }

    RTPrintf("Test returned: %s (%u)\n", dwErr == ERROR_SUCCESS ? "SUCCESS" : "FAILURE", dwErr);
    return dwErr == ERROR_SUCCESS ? 0 : 1;
}

