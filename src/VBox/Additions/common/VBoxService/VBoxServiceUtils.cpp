
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
#include <windows.h>
#endif

#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/string.h>

#include <VBox/VBoxGuest.h>
#include "VBoxServiceInternal.h"


#ifdef VBOX_WITH_GUEST_PROPS
int VboxServiceWriteProp(uint32_t uiClientID, const char *pszKey, const char *pszValue)
{
    int rc = VINF_SUCCESS;
    Assert(pszKey);
    /* Not checking for a valid pszValue is intentional. */

    char szKeyTemp [FILENAME_MAX] = {0};
    char *pszValueTemp = NULL;

    /* Append base path. */
    RTStrPrintf(szKeyTemp, sizeof(szKeyTemp), "/VirtualBox/%s", pszKey); /** @todo r=bird: Why didn't you hardcode this into the strings before calling this function? */

    if (pszValue != NULL)
    {
        rc = RTStrCurrentCPToUtf8(&pszValueTemp, pszValue);
        if (!RT_SUCCESS(rc)) 
        {
            VBoxServiceError("vboxVMInfoThread: Failed to convert the value name \"%s\" to Utf8! Error: %Rrc\n", pszValue, rc);
            goto cleanup;
        }
    }

    rc = VbglR3GuestPropWriteValue(uiClientID, szKeyTemp, ((pszValue == NULL) || (0 == strlen(pszValue))) ? NULL : pszValueTemp);
    if (!RT_SUCCESS(rc))
    {
        VBoxServiceError("Failed to store the property \"%s\"=\"%s\"! ClientID: %d, Error: %Rrc\n", szKeyTemp, pszValueTemp, uiClientID, rc);
        goto cleanup;
    }

    if ((pszValueTemp != NULL) && (strlen(pszValueTemp) > 0))
        VBoxServiceVerbose(3, "Property written: %s = %s\n", szKeyTemp, pszValueTemp);
    else
        VBoxServiceVerbose(3, "Property deleted: %s\n", szKeyTemp);

cleanup:

    RTStrFree(pszValueTemp);
    return rc;
}


int VboxServiceWritePropInt(uint32_t uiClientID, const char *pszKey, int32_t iValue)
{
    Assert(pszKey);

    char szBuffer[32] = {0};
    RTStrPrintf(szBuffer, sizeof(szBuffer), "%ld", iValue);
    return VboxServiceWriteProp(uiClientID, pszKey, szBuffer);
}
#endif /* VBOX_WITH_GUEST_PROPS */


#ifdef RT_OS_WINDOWS
/** @todo Use TCHAR here instead of LPC*STR crap. */
BOOL VboxServiceGetFileString(LPCWSTR pszFileName, 
                              LPWSTR pszBlock, 
                              LPWSTR pszString, 
                              PUINT puiSize)
{
    DWORD dwHandle, dwLen = 0;
    UINT uiDataLen = 0;
    LPTSTR lpData = NULL;
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
        VBoxServiceError("No file information found! File = %ls, Error: %ld\n", pszFileName, GetLastError());
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
            UINT uiSize = uiValueLen * sizeof(TCHAR);

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


BOOL VboxServiceGetFileVersion(LPCWSTR pszFileName,
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
    TCHAR szValueUTF16[_MAX_PATH] = {0};
    char szValueUTF8[_MAX_PATH] = {0};
    char *pszValueUTF8  = szValueUTF8;
    UINT uiSize = _MAX_PATH;
    int r = 0;

    bRet = VboxServiceGetFileString(pszFileName, TEXT("\\StringFileInfo\\040904b0\\FileVersion"), szValueUTF16, &uiSize);
    if (bRet)
    {
        r = RTUtf16ToUtf8Ex(szValueUTF16, uiSize, &pszValueUTF8, _MAX_PATH, NULL);
        sscanf(szValueUTF8, "%ld.%ld.%ld.%ld", pdwMajor, pdwMinor, pdwBuildNumber, pdwRevisionNumber);
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
            if((bRet = VerQueryValue(lpData, TEXT("\\"), (LPVOID*)&pFileInfo, (PUINT)&BufLen)))
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


BOOL VboxServiceGetFileVersionString(LPCWSTR pszPath, LPCWSTR pszFileName, char* pszVersion, UINT uiSize)
{
    BOOL bRet = FALSE;
    TCHAR szFullPath[_MAX_PATH] = {0};
    TCHAR szValueUTF16[_MAX_PATH] = {0};
    char szValueUTF8[_MAX_PATH] = {0};
    int r = 0;

    swprintf(szFullPath, 4096, TEXT("%s\\%s"), pszPath, pszFileName); /** @todo here as well. */

    DWORD dwMajor, dwMinor, dwBuild, dwRev;

    bRet = VboxServiceGetFileVersion(szFullPath, &dwMajor, &dwMinor, &dwBuild, &dwRev);
    if (bRet)
        RTStrPrintf(pszVersion, uiSize, "%ld.%ld.%ldr%ld", dwMajor, dwMinor, dwBuild, dwRev);
    else
        RTStrPrintf(pszVersion, uiSize, "-");

    return bRet;
}
#endif /* !RT_OS_WINDOWS */
