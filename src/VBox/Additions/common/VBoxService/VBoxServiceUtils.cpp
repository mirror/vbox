/* $Id$ */
/** @file
 * VBoxServiceUtils - Some utility functions.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#ifdef RT_OS_WINDOWS
# include <Windows.h>
#endif

#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/string.h>

#include <VBox/VBoxGuestLib.h>
#include "VBoxServiceInternal.h"


#ifdef VBOX_WITH_GUEST_PROPS
int VBoxServiceWritePropF(uint32_t u32ClientId, const char *pszName, const char *pszValueFormat, ...)
{
    char *pszNameUTF8;
    int rc = RTStrCurrentCPToUtf8(&pszNameUTF8, pszName);
    if (RT_SUCCESS(rc))
    {
        if (pszValueFormat != NULL)
        {
            va_list va;
            va_start(va, pszValueFormat);
            VBoxServiceVerbose(3, "Writing guest property \"%s\"=\"%s\"\n", pszNameUTF8, pszValueFormat);
            rc = VbglR3GuestPropWriteValueV(u32ClientId, pszNameUTF8, pszValueFormat, va);
            if (RT_FAILURE(rc))
                 VBoxServiceError("Error writing guest property \"%s\"=\"%s\" (rc=%Rrc)\n", pszNameUTF8, pszValueFormat, rc);
            va_end(va);
        }
        else
            rc = VbglR3GuestPropWriteValue(u32ClientId, pszNameUTF8, NULL);
        RTStrFree(pszNameUTF8);
    }
    return rc;
}
#endif /* VBOX_WITH_GUEST_PROPS */


#ifdef RT_OS_WINDOWS
BOOL VBoxServiceGetFileString(const char* pszFileName,
                              char* pszBlock,
                              char* pszString,
                              PUINT puiSize)
{
    DWORD dwHandle, dwLen = 0;
    UINT uiDataLen = 0;
    char* lpData = NULL;
    UINT uiValueLen = 0;
    LPTSTR lpValue = NULL;
    BOOL bRet = FALSE;

    Assert(pszFileName);
    Assert(pszBlock);
    Assert(pszString);
    Assert(puiSize > 0);

    /* The VS_FIXEDFILEINFO structure contains version information about a file.
       This information is language and code page independent. */
    VS_FIXEDFILEINFO *pFileInfo = NULL;
    dwLen = GetFileVersionInfoSize(pszFileName, &dwHandle);

    if (!dwLen)
    {
        VBoxServiceError("No file information found! File = %s, Error: %ld\n", pszFileName, GetLastError());
        return FALSE;
    }

    lpData = (LPTSTR) RTMemTmpAlloc(dwLen);
    if (!lpData)
    {
        VBoxServiceError("Could not allocate temp buffer!\n");
        return FALSE;
    }

    if (GetFileVersionInfo(pszFileName, dwHandle, dwLen, lpData))
    {
        if((bRet = VerQueryValue(lpData, pszBlock, (LPVOID*)&lpValue, (PUINT)&uiValueLen)))
        {
            UINT uiSize = uiValueLen * sizeof(char);

            if(uiSize > *puiSize)
                uiSize = *puiSize;

            ZeroMemory(pszString, *puiSize);
            memcpy(pszString, lpValue, uiSize);
        }
        else VBoxServiceError("Could not query value!\n");
    }
    else VBoxServiceError("Could not get file version info!\n");

    RTMemFree(lpData);
    return bRet;
}


BOOL VBoxServiceGetFileVersion(const char* pszFileName,
                               DWORD* pdwMajor,
                               DWORD* pdwMinor,
                               DWORD* pdwBuildNumber,
                               DWORD* pdwRevisionNumber)
{
    DWORD dwHandle, dwLen = 0;
    UINT BufLen = 0;
    LPTSTR lpData = NULL;
    BOOL bRet = FALSE;

    Assert(pszFileName);
    Assert(pdwMajor);
    Assert(pdwMinor);
    Assert(pdwBuildNumber);
    Assert(pdwRevisionNumber);

    /* The VS_FIXEDFILEINFO structure contains version information about a file.
       This information is language and code page independent. */
    VS_FIXEDFILEINFO *pFileInfo = NULL;
    dwLen = GetFileVersionInfoSize(pszFileName, &dwHandle);

    /* Try own fields defined in block "\\StringFileInfo\\040904b0\\FileVersion". */
    char szValue[_MAX_PATH] = {0};
    char *pszValue  = szValue;
    UINT uiSize = _MAX_PATH;
    int r = 0;

    bRet = VBoxServiceGetFileString(pszFileName, "\\StringFileInfo\\040904b0\\FileVersion", szValue, &uiSize);
    if (bRet)
    {
        sscanf(pszValue, "%ld.%ld.%ld.%ld", pdwMajor, pdwMinor, pdwBuildNumber, pdwRevisionNumber);
    }
    else if (dwLen > 0)
    {
        /* Try regular fields - this maybe is not file provided by VBox! */
        lpData = (LPTSTR) RTMemTmpAlloc(dwLen);
        if (!lpData)
        {
            VBoxServiceError("Could not allocate temp buffer!\n");
            return FALSE;
        }

        if (GetFileVersionInfo(pszFileName, dwHandle, dwLen, lpData))
        {
            if((bRet = VerQueryValue(lpData, "\\", (LPVOID*)&pFileInfo, (PUINT)&BufLen)))
            {
                *pdwMajor = HIWORD(pFileInfo->dwFileVersionMS);
                *pdwMinor = LOWORD(pFileInfo->dwFileVersionMS);
                *pdwBuildNumber = HIWORD(pFileInfo->dwFileVersionLS);
                *pdwRevisionNumber = LOWORD(pFileInfo->dwFileVersionLS);
            }
            else VBoxServiceError("Could not query file information value!\n");
        }
        else VBoxServiceError("Could not get file version info!\n");

        RTMemFree(lpData);
    }
    return bRet;
}


BOOL VBoxServiceGetFileVersionString(const char* pszPath, const char* pszFileName, char* pszVersion, UINT uiSize)
{
    BOOL bRet = FALSE;
    char szFullPath[_MAX_PATH] = {0};
    char szValue[_MAX_PATH] = {0};
    int r = 0;

    RTStrPrintf(szFullPath, 4096, "%s\\%s", pszPath, pszFileName);

    DWORD dwMajor, dwMinor, dwBuild, dwRev;

    bRet = VBoxServiceGetFileVersion(szFullPath, &dwMajor, &dwMinor, &dwBuild, &dwRev);
    if (bRet)
        RTStrPrintf(pszVersion, uiSize, "%ld.%ld.%ldr%ld", dwMajor, dwMinor, dwBuild, dwRev);
    else
        RTStrPrintf(pszVersion, uiSize, "-");

    return bRet;
}
#endif /* !RT_OS_WINDOWS */

