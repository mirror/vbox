/* $Id$ */
/** @file
 * VBoxWindowsAdditions - The Windows Guest Additions Loader
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <stdio.h>
#include <tchar.h>

typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);

LPFN_ISWOW64PROCESS fnIsWow64Process;

BOOL IsWow64 ()
{
    BOOL bIsWow64 = FALSE;
    fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle(TEXT("kernel32")), "IsWow64Process");

    if (NULL != fnIsWow64Process)
    {
        if (!fnIsWow64Process(GetCurrentProcess(), &bIsWow64))
        {
            _tprintf(_T("ERROR: Could not determine process type!\n"));

            /* Error in retrieving process type - assume that we're running on 32bit. */
            return FALSE;
        }
    }
    return bIsWow64;
}

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    int iRet = 0;

    LPWSTR *pszArgList = NULL;
    int nArgs = 0;

    TCHAR szCurDir[_MAX_PATH + 1] = { 0 };
    GetCurrentDirectory(_MAX_PATH, szCurDir);

    SHELLEXECUTEINFOW TempInfo = { 0 };
    TempInfo.cbSize = sizeof(SHELLEXECUTEINFOW);

    TCHAR szModule[_MAX_PATH + 1] = { 0 };
    TCHAR szApp[_MAX_PATH + 1] = { 0 };
    TCHAR szProg[_MAX_PATH + 1] = { 0 };

    pszArgList = CommandLineToArgvW(GetCommandLineW(), &nArgs);
    if (0 == GetModuleFileName(NULL, szModule, _MAX_PATH))
    {
        /* Module not found, use a default name. */
        _stprintf(szModule, _T("%ws"), (pszArgList != NULL) ? pszArgList[0] : _T("VBoxWindowsAdditions.exe"));
    }

    TCHAR* pcExt = wcsrchr(szModule, _T('.'));
    if (NULL != pcExt)
        wcsncpy(szApp, szModule, (pcExt - szModule));

    if (IsWow64()) /* 64bit Windows. */
    {
        _stprintf(szProg, _T("%ws-amd64.exe"), szApp);
    }
    else /* 32bit Windows. */
    {
        _stprintf(szProg, _T("%ws-x86.exe"), szApp);
    }

    /* Construct parameter list. */
    TCHAR *pszParams = (TCHAR*)LocalAlloc(LPTR, _MAX_PATH*sizeof(TCHAR));
    TCHAR szDelim = _T(' ');

    if (pszParams)
    {
        wcsncat(pszParams, szProg,
                        __min(wcslen(szProg),_MAX_PATH-wcslen(pszParams)));
        wcsncat(pszParams, &szDelim,
                        __min(1,_MAX_PATH-wcslen(pszParams)));

        if (nArgs > 1)
        {
            for (int i=0; i<nArgs-1; i++)
            {
                if (i > 0)
                {
                    wcsncat(pszParams, &szDelim,
                        __min(1,_MAX_PATH-wcslen(pszParams)));
                }
                wcsncat(pszParams, pszArgList[i+1],
                        __min(wcslen(pszArgList[i+1]),_MAX_PATH-wcslen(pszParams)));
            }
        }
    }

    /* Struct for ShellExecute. */
    TempInfo.fMask = 0;
    TempInfo.hwnd = NULL;
    TempInfo.lpVerb =L"runas" ;
    TempInfo.lpFile = szProg;
    TempInfo.lpParameters = pszParams;
    TempInfo.lpDirectory = szCurDir;
    TempInfo.nShow = SW_NORMAL;

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD dwRes = 0;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    dwRes = CreateProcessW(szProg, pszParams, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    if (!dwRes && GetLastError() == 740) /* 740 = ERROR_ELEVATION_REQUIRED -> Only for Windows Vista. */
    {
        if (FALSE == ::ShellExecuteExW(&TempInfo))
        {
            _tprintf (_T("ERROR: Could not launch program! Code: %ld\n"), GetLastError());
            iRet = 1;
        }
    }

    if (pszParams)
        LocalFree(pszParams);

    if (pszArgList)
        LocalFree(pszArgList);

    return iRet;
}

