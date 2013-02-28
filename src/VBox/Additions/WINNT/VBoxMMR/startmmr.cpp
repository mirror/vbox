/* $Id$ */
/** @file
 * VBoxMMR - Multimedia Redirection
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
 */

#include <stdio.h>
#include <tchar.h>
#include <windows.h>

#include <iprt/initterm.h>

#include <VBox/Log.h>
#include <VBox/VBoxGuestLib.h>

const char *g_pszMMRDLL        = "VBoxMMRHook";
const char *g_pszMMRPROC       = "CBTProc";
const WCHAR *g_pwszMMRFlags    = L"VBoxMMR";

const WCHAR *g_pwszMMRAdditions =
    L"SOFTWARE\\Oracle\\VirtualBox Guest Additions";

const DWORD g_dwMMREnabled = 0x00000001;

HANDLE g_hCtrlEvent;

BOOL MMRIsEnabled()
{
    LONG lResult;
    HKEY hKey;
    DWORD dwType = 0;
    DWORD dwValue = 0;
    DWORD dwSize = sizeof(dwValue);

    BOOL fEnabled = TRUE;

    lResult = RegOpenKeyExW(
        HKEY_LOCAL_MACHINE, g_pwszMMRAdditions, 0, KEY_QUERY_VALUE, &hKey);

    if (lResult == ERROR_SUCCESS)
    {
        lResult = RegQueryValueExW(
            hKey, g_pwszMMRFlags, NULL, &dwType, (BYTE *) &dwValue, &dwSize);

        RegCloseKey(hKey);

        if (lResult == ERROR_SUCCESS &&
            dwSize == sizeof(dwValue) &&
            dwType == REG_DWORD)
        {
            fEnabled = g_dwMMREnabled & dwValue;
            LogRel(("VBoxMMR: Registry setting: %d\n", dwValue));
        }
    }

    return fEnabled;
}

BOOL CtrlHandler(DWORD type)
{
    SetEvent(g_hCtrlEvent);
    return TRUE;
}

int APIENTRY WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow)
{
    HINSTANCE    hMod  = NULL;
    HHOOK        hHook = NULL;
    HOOKPROC     pHook = NULL;

    int rc = RTR3InitExeNoArguments(0);
    if (RT_FAILURE(rc))
    {
        LogRel(("VBoxMMR: Error initializing RT runtime: %d\n", rc));
        return rc;
    }

    rc = VbglR3Init();
    if (RT_FAILURE(rc))
    {
        LogRel(("VBoxMMR: Error initializing VbglR3 runtime: %d\n", rc));
        return rc;
    }

    if (MMRIsEnabled())
    {
        hMod = LoadLibraryA(g_pszMMRDLL);
        if (hMod == NULL)
        {
            LogRel(("VBoxMMR: Hooking library not found\n"));
            return VERR_NOT_FOUND;
        }

        pHook = (HOOKPROC) GetProcAddress(hMod, g_pszMMRPROC);
        if (pHook == NULL)
        {
            LogRel(("VBoxMMR: Hooking proc not found\n"));
            FreeLibrary(hMod);
            return VERR_NOT_FOUND;
        }

        hHook = SetWindowsHookEx(WH_CBT, pHook, hMod, 0);
        if (hHook == NULL)
        {
            int rc = RTErrConvertFromWin32(GetLastError());
            LogRel(("VBoxMMR: Error installing hooking proc: %d\n", rc));
            FreeLibrary(hMod);
            return rc;
        }

        g_hCtrlEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

        if (SetConsoleCtrlHandler((PHANDLER_ROUTINE) CtrlHandler, TRUE))
        {
            WaitForSingleObject(g_hCtrlEvent, INFINITE);
        }
        else
        {
            int rc = RTErrConvertFromWin32(GetLastError());
            LogRel(("VBoxMMR: Error installing ctrl handler: %d\n", rc));
        }

        CloseHandle(g_hCtrlEvent);

        UnhookWindowsHookEx(hHook);
        FreeLibrary(hMod);
    }

    VbglR3Term();

    return 0;
}
