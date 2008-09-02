
/* $Id: VBoxUtil.cpp 33909 2008-07-31 09:30:59Z andy $ */
/** @file
 * VBoxUtil - Some tool functions.
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */

#include "VBoxService.h"
#include "VBoxUtils.h"

BOOL vboxGetFileVersion (LPCWSTR a_pszFileName,
                         DWORD* a_pdwMajor, DWORD* a_pdwMinor, DWORD* a_pdwBuildNumber, DWORD* a_pdwRevisionNumber)
{
    DWORD dwHandle, dwLen = 0;
    UINT BufLen = 0;
    LPTSTR lpData = NULL;
    BOOL bRet = FALSE;

    Assert(a_pszFileName);
    Assert(a_pdwMajor);
    Assert(a_pdwMinor);
    Assert(a_pdwBuildNumber);
    Assert(a_pdwRevisionNumber);

    Log(("VBoxService: vboxGetFileVersionString: File = %ls\n", a_pszFileName));

    /* The VS_FIXEDFILEINFO structure contains version information about a file.
       This information is language and code page independent. */
    VS_FIXEDFILEINFO *pFileInfo = NULL;
    dwLen = GetFileVersionInfoSize(a_pszFileName, &dwHandle);

    Log(("VBoxService: vboxGetFileVersion: File version info size = %ld\n", dwLen));

    /* Try own fields defined in block "\\StringFileInfo\\040904b0\\FileVersion". */
    TCHAR szValueUTF16[_MAX_PATH] = {0};
    char szValueUTF8[_MAX_PATH] = {0};
    char *pszValueUTF8  = szValueUTF8;
    UINT uiSize = _MAX_PATH;
    int r = 0;

    bRet = vboxGetFileString(a_pszFileName, TEXT("\\StringFileInfo\\040904b0\\FileVersion"), szValueUTF16, &uiSize);
    if (bRet)
    {
        r = RTUtf16ToUtf8Ex(szValueUTF16, uiSize, &pszValueUTF8, _MAX_PATH, NULL);
        sscanf(szValueUTF8, "%ld.%ld.%ld.%ld", a_pdwMajor, a_pdwMinor, a_pdwBuildNumber, a_pdwRevisionNumber);
    }
    else if (dwLen > 0)
    {
        /* Try regular fields - this maybe is not file provided by VBox! */
        lpData = (LPTSTR) RTMemTmpAlloc(dwLen);
        if (!lpData)
        {
            Log(("VBoxService: vboxGetFileVersion: Could not allocate temp buffer!\n"));
            return FALSE;
        }

        if (GetFileVersionInfo(a_pszFileName, dwHandle, dwLen, lpData))
        {
            if((bRet = VerQueryValue(lpData, TEXT("\\"), (LPVOID*)&pFileInfo, (PUINT)&BufLen)))
            {
                *a_pdwMajor = HIWORD(pFileInfo->dwFileVersionMS);
                *a_pdwMinor = LOWORD(pFileInfo->dwFileVersionMS);
                *a_pdwBuildNumber = HIWORD(pFileInfo->dwFileVersionLS);
                *a_pdwRevisionNumber = LOWORD(pFileInfo->dwFileVersionLS);
            }
            else Log(("VBoxService: vboxGetFileVersion: Could not query value!\n"));
        }
        else Log(("VBoxService: vboxGetFileVersion: Could not get file version info!\n"));

        RTMemFree(lpData);
    }
    return bRet;
}

BOOL vboxGetFileString (LPCWSTR a_pszFileName, LPWSTR a_pszBlock, LPWSTR a_pszString, PUINT a_puiSize)
{
    DWORD dwHandle, dwLen = 0;
    UINT uiDataLen = 0;
    LPTSTR lpData = NULL;
    UINT uiValueLen = 0;
    LPTSTR lpValue = NULL;
    BOOL bRet = FALSE;

    Assert(a_pszFileName);
    Assert(a_pszBlock);
    Assert(a_pszString);
    Assert(a_puiSize > 0);

    Log(("VBoxService: vboxGetFileString: File = %ls\n", a_pszFileName));

    /* The VS_FIXEDFILEINFO structure contains version information about a file.
       This information is language and code page independent. */
    VS_FIXEDFILEINFO *pFileInfo = NULL;
    dwLen = GetFileVersionInfoSize(a_pszFileName, &dwHandle);

    if (!dwLen)
    {
        Log(("VBoxService: vboxGetFileString: No file information found! File = %ls, Error: %ld\n", a_pszFileName, GetLastError()));
        return FALSE;   /* No version information available. */
    }

    lpData = (LPTSTR) RTMemTmpAlloc(dwLen);
    if (!lpData)
    {
        Log(("VBoxService: vboxGetFileString: Could not allocate temp buffer!\n"));
        return FALSE;
    }

    if (GetFileVersionInfo(a_pszFileName, dwHandle, dwLen, lpData))
    {
        if((bRet = VerQueryValue(lpData, a_pszBlock, (LPVOID*)&lpValue, (PUINT)&uiValueLen)))
        {
            UINT uiSize = uiValueLen * sizeof(TCHAR);

            if(uiSize > *a_puiSize)
                uiSize = *a_puiSize;

            ZeroMemory(a_pszString, *a_puiSize);
            memcpy(a_pszString, lpValue, uiSize);

            Log(("VBoxService: vboxGetFileString: Block = %ls, Size = %d, Value = %ls\n", a_pszBlock, uiValueLen, a_pszString));
        }
        else Log(("VBoxService: vboxGetFileString: Could not query value!\n"));
    }
    else Log(("VBoxService: vboxGetFileString: Could not get file version info!\n"));

    RTMemFree(lpData);
    return bRet;
}

BOOL vboxGetFileVersionString (LPCWSTR a_pszPath, LPCWSTR a_pszFileName, char* a_pszVersion, UINT a_uiSize)
{
    BOOL bRet = FALSE;
    TCHAR szFullPath[4096] = {0};
    TCHAR szValueUTF16[_MAX_PATH] = {0};
    char szValueUTF8[_MAX_PATH] = {0};
    UINT uiSize = _MAX_PATH;
    int r = 0;

    swprintf(szFullPath, 4096, TEXT("%s\\%s"), a_pszPath, a_pszFileName);

    DWORD dwMajor, dwMinor, dwBuild, dwRev;

    bRet = vboxGetFileVersion(szFullPath, &dwMajor, &dwMinor, &dwBuild, &dwRev);
    if (bRet)
        RTStrPrintf(a_pszVersion, a_uiSize, "%ld.%ld.%ldr%ld", dwMajor, dwMinor, dwBuild, dwRev);
    else
        RTStrPrintf(a_pszVersion, a_uiSize, "-");

    return bRet;
}
