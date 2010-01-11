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
/**
 * Reads a guest property.
 *
 * @returns VBox status code, fully bitched.
 *
 * @param   u32ClientId         The HGCM client ID for the guest property session.
 * @param   pszPropName         The property name.
 * @param   ppszValue           Where to return the value.  This is always set
 *                              to NULL.  Free it using RTStrFree().
 * @param   ppszFlags           Where to return the value flags. Free it
 *                              using RTStrFree().  Optional.
 * @param   puTimestamp         Where to return the timestamp.  This is only set
 *                              on success.  Optional.
 */
int VBoxServiceReadProp(uint32_t u32ClientId, const char *pszPropName, char **ppszValue, char **ppszFlags, uint64_t *puTimestamp)
{
    size_t  cbBuf = _1K;
    void   *pvBuf = NULL;
    int     rc;

    *ppszValue = NULL;

    for (unsigned cTries = 0; cTries < 10; cTries++)
    {
        /*
         * (Re-)Allocate the buffer and try read the property.
         */
        RTMemFree(pvBuf);
        pvBuf = RTMemAlloc(cbBuf);
        if (!pvBuf)
        {
            VBoxServiceError("Guest Property: Failed to allocate %zu bytes\n", cbBuf);
            rc = VERR_NO_MEMORY;
            break;
        }
        char    *pszValue;
        char    *pszFlags;
        uint64_t uTimestamp;
        rc = VbglR3GuestPropRead(u32ClientId, pszPropName,
                                 pvBuf, cbBuf,
                                 &pszValue, &uTimestamp, &pszFlags, NULL);
        if (RT_FAILURE(rc))
        {
            if (rc == VERR_BUFFER_OVERFLOW)
            {
                /* try again with a bigger buffer. */
                cbBuf *= 2;
                continue;
            }
            if (rc == VERR_NOT_FOUND)
                VBoxServiceVerbose(2, "Guest Property: %s not found\n", pszPropName);
            else
                VBoxServiceError("Guest Property: Failed to query \"%s\": %Rrc\n", pszPropName, rc);
            break;
        }

        VBoxServiceVerbose(2, "Guest Property: Read \"%s\" = \"%s\", timestamp %RU64n\n",
                           pszPropName, pszValue, uTimestamp);
        *ppszValue = RTStrDup(pszValue);
        if (!*ppszValue)
        {
            VBoxServiceError("Guest Property: RTStrDup failed for \"%s\"\n", pszValue);
            rc = VERR_NO_MEMORY;
            break;
        }

        if (puTimestamp)
            *puTimestamp = uTimestamp;
        if (ppszFlags)
            *ppszFlags = RTStrDup(pszFlags);
        break; /* done */
    }

    RTMemFree(pvBuf);
    return rc;
}


/**
 * Reads a guest property as a 32-bit value.
 *
 * @returns VBox status code, fully bitched.
 *
 * @param   u32ClientId         The HGCM client ID for the guest property session.
 * @param   pszPropName         The property name.
 * @param   pu32                Where to store the 32-bit value.
 *
 */
int VBoxServiceReadPropUInt32(uint32_t u32ClientId, const char *pszPropName, uint32_t *pu32, uint32_t u32Min, uint32_t u32Max)
{
    char *pszValue;
    int rc = VBoxServiceReadProp(u32ClientId, pszPropName, &pszValue,
        NULL /* ppszFlags */, NULL /* puTimestamp */);
    if (RT_SUCCESS(rc))
    {
        AssertPtr(pu32);
        char *pszNext;
        rc = RTStrToUInt32Ex(pszValue, &pszNext, 0, pu32);
        if (   RT_SUCCESS(rc)
            && (*pu32 < u32Min || *pu32 > u32Max))
        {
            rc = VBoxServiceError("The guest property value %s = %RU32 is out of range [%RU32..%RU32].\n",
                                  pszPropName, *pu32, u32Min, u32Max);
        }
        RTStrFree(pszValue);
    }
    return rc;
}


/**
 * Wrapper around VbglR3GuestPropWriteValue that does value formatting and
 * logging.
 *
 * @returns VBox status code. Errors will be logged.
 *
 * @param   u32ClientId     The HGCM client ID for the guest property session.
 * @param   pszName         The property name.
 * @param   pszValueFormat  The property format string.  If this is NULL then
 *                          the property will be deleted (if possible).
 * @param   ...             Format arguments.
 */
int VBoxServiceWritePropF(uint32_t u32ClientId, const char *pszName, const char *pszValueFormat, ...)
{
    AssertPtr(pszName);
    int rc;
    if (pszValueFormat != NULL)
    {
        va_list va;
        va_start(va, pszValueFormat);

        char *pszValue = RTStrAPrintf2V(pszValueFormat, va);
        AssertPtr(pszValue);
        VBoxServiceVerbose(3, "Writing guest property \"%s\" = \"%s\"\n", pszName, pszValue);
        RTStrFree(pszValue);

        rc = VbglR3GuestPropWriteValueV(u32ClientId, pszName, pszValueFormat, va);
        va_end(va);
        if (RT_FAILURE(rc))
             VBoxServiceError("Error writing guest property \"%s\" (rc=%Rrc)\n", pszName, rc);
    }
    else
    {
        VBoxServiceVerbose(3, "Deleting guest property \"%s\"\n", pszName);   
        rc = VbglR3GuestPropWriteValue(u32ClientId, pszName, NULL);
        if (RT_FAILURE(rc))
            VBoxServiceError("Error deleting guest property \"%s\" (rc=%Rrc)\n", pszName, rc);
    }
    return rc;
}
#endif /* VBOX_WITH_GUEST_PROPS */


#ifdef RT_OS_WINDOWS
/** @todo return an iprt status code instead of BOOL */
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
        /* Don't print this to release log -- this confuses people if a file
         * isn't present because it's optional / was not installed intentionally. */
        VBoxServiceVerbose(3, "No file information found! File = %s, Error: %Rrc\n",
            pszFileName, RTErrConvertFromWin32(GetLastError()));
        return FALSE;
    }

    lpData = (LPTSTR) RTMemTmpAlloc(dwLen);
    if (!lpData)
    {
        VBoxServiceError("Could not allocate temp buffer for file string lookup!\n");
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
        else VBoxServiceVerbose(3, "No file string value for \"%s\" in file \"%s\" available!\n", pszBlock, pszFileName);
    }
    else VBoxServiceVerbose(3, "No file version table for file \"%s\" available!\n", pszFileName);

    RTMemFree(lpData);
    return bRet;
}


/** @todo return an iprt status code instead of BOOL */
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
            VBoxServiceError("Could not allocate temp buffer for file version string!\n");
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
                bRet = TRUE;
            }
            else VBoxServiceVerbose(3, "No file version value for file \"%s\" available!\n", pszFileName);
        }
        else VBoxServiceVerbose(3, "No file version struct for file \"%s\" available!\n", pszFileName);

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

