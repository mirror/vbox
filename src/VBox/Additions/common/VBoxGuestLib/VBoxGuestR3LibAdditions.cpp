/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, Additions Info.
 */

/*
 * Copyright (C) 2007-2011 Oracle Corporation
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/mem.h>
#include <iprt/string.h>
#include <VBox/log.h>
#include <VBox/version.h>
#include "VBGLR3Internal.h"


/**
 * Fallback for VbglR3GetAdditionsVersion.
 */
static int vbglR3GetAdditionsCompileTimeVersion(char **ppszVer, char **ppszVerEx, char **ppszRev)
{
    int rc = VINF_SUCCESS;
    if (ppszVer)
        rc = RTStrDupEx(ppszVer, VBOX_VERSION_STRING_RAW);
    if (RT_SUCCESS(rc))
    {
        if (ppszVerEx)
            rc = RTStrDupEx(ppszVerEx, VBOX_VERSION_STRING);
        if (RT_SUCCESS(rc))
        {
            if (ppszRev)
            {
#if 0
                char szRev[64];
                RTStrPrintf(szRev, sizeof(szRev), "%d", VBOX_SVN_REV);
                rc = RTStrDupEx(ppszRev, szRev);
#else
                rc = RTStrDupEx(ppszRev, RT_XSTR(VBOX_SVN_REV));
#endif
            }
            if (RT_SUCCESS(rc))
                return VINF_SUCCESS;

            /* bail out: */
        }
        if (ppszVerEx)
        {
            RTStrFree(*ppszVerEx);
            *ppszVerEx = NULL;
        }
    }
    if (ppszVer)
    {
        RTStrFree(*ppszVer);
        *ppszVer = NULL;
    }
    return rc;
}

#ifdef RT_OS_WINDOWS

/**
 * Looks up the storage path handle (registry).
 *
 * @returns IPRT status value
 * @param   hKey        Receives storage path handle on success.
 *                      The returned handle must be closed by vbglR3CloseAdditionsWinStoragePath().
 */
static int vbglR3QueryAdditionsWinStoragePath(PHKEY phKey)
{
    /*
     * Try get the *installed* version first.
     */
    LONG r;

    /* Check the built in vendor path first. */
    char szPath[255];
    RTStrPrintf(szPath, sizeof(szPath), "SOFTWARE\\%s\\VirtualBox Guest Additions", VBOX_VENDOR_SHORT);
    r = RegOpenKeyEx(HKEY_LOCAL_MACHINE, szPath, 0, KEY_READ, phKey);
# ifdef RT_ARCH_AMD64
    if (r != ERROR_SUCCESS)
    {
        /* Check Wow6432Node. */
        RTStrPrintf(szPath, sizeof(szPath), "SOFTWARE\\Wow6432Node\\%s\\VirtualBox Guest Additions", VBOX_VENDOR_SHORT);
        r = RegOpenKeyEx(HKEY_LOCAL_MACHINE, szPath, 0, KEY_READ, phKey);
    }
# endif

    /* Check the "Sun" path first. */
    if (r != ERROR_SUCCESS)
    {
        r = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Sun\\VirtualBox Guest Additions", 0, KEY_READ, phKey);
# ifdef RT_ARCH_AMD64
        if (r != ERROR_SUCCESS)
        {
            /* Check Wow6432Node (for new entries). */
            r = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Wow6432Node\\Sun\\VirtualBox Guest Additions", 0, KEY_READ, phKey);
        }
# endif
    }

    /* Still no luck? Then try the old "Sun xVM" paths ... */
    if (r != ERROR_SUCCESS)
    {
        r = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Sun\\xVM VirtualBox Guest Additions", 0, KEY_READ, phKey);
# ifdef RT_ARCH_AMD64
        if (r != ERROR_SUCCESS)
        {
            /* Check Wow6432Node (for new entries). */
            r = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Wow6432Node\\Sun\\xVM VirtualBox Guest Additions", 0, KEY_READ, phKey);
        }
# endif
    }
    return RTErrConvertFromWin32(r);
}


/**
 * Closes the storage path handle (registry).
 *
 * @returns IPRT status value
 * @param   hKey        Handle to close, retrieved by
 *                      vbglR3QueryAdditionsWinStoragePath().
 */
static int vbglR3CloseAdditionsWinStoragePath(HKEY hKey)
{
    return RTErrConvertFromWin32(RegCloseKey(hKey));
}

#endif /* RT_OS_WINDOWS */

/**
 * Reports the Guest Additions status of a certain facility to the host.
 *
 * @returns IPRT status value
 * @param   enmFacility     The facility to report the status on.
 * @param   enmStatus       The new status of the facility.
 * @param   fReserved       Reserved for future use (what?).
 */
VBGLR3DECL(int) VbglR3ReportAdditionsStatus(VBoxGuestFacilityType enmFacility,
                                            VBoxGuestFacilityStatus enmStatusCurrent,
                                            uint32_t fReserved)
{
    VMMDevReportGuestStatus Report;
    RT_ZERO(Report);
    int rc = vmmdevInitRequest((VMMDevRequestHeader*)&Report, VMMDevReq_ReportGuestStatus);
    if (RT_SUCCESS(rc))
    {
        Report.guestStatus.facility = enmFacility;
        Report.guestStatus.status   = enmStatusCurrent;
        Report.guestStatus.flags    = fReserved;

        rc = vbglR3GRPerform(&Report.header);
    }
    return rc;
}

#ifdef RT_OS_WINDOWS

/**
 * Queries a string value from a specified registry key.
 *
 * @return  IPRT status code.
 * @param   hKey                    Handle of registry key to use.
 * @param   pszValName              Value name to query value from.
 * @param   pszBuffer               Pointer to buffer which the queried string value gets stored into.
 * @param   cchBuffer               Size (in bytes) of buffer.
 */
int vbglR3QueryRegistryString(HKEY hKey, const char *pszValName, char *pszBuffer, size_t cchBuffer)
{
    AssertReturn(pszValName, VERR_INVALID_PARAMETER);
    AssertReturn(pszBuffer, VERR_INVALID_POINTER);
    AssertReturn(cchBuffer, VERR_INVALID_PARAMETER);

    int rc = VINF_SUCCESS;

    DWORD dwType;
    DWORD dwSize = (DWORD)cchBuffer;
    LONG lRet = RegQueryValueEx(hKey, pszValName, NULL, &dwType, (BYTE*)(LPCTSTR)pszBuffer, &dwSize);
    if (lRet == ERROR_SUCCESS)
    {
        rc = dwType == REG_SZ
                     ? VINF_SUCCESS : VERR_INVALID_PARAMETER;
    }
    else
        rc = RTErrConvertFromWin32(lRet);
    return rc;
}

#endif /* RT_OS_WINDOWS */

/**
 * Retrieves the installed Guest Additions version and/or revision.
 *
 * @returns IPRT status value
 * @param   ppszVer     Receives pointer of allocated raw version string
 *                      (major.minor.build). NULL is accepted. The returned
 *                      pointer must be freed using RTStrFree().*
 * @param   ppszVerExt  Receives pointer of allocated full version string
 *                      (raw version + vendor suffix(es)). NULL is
 *                      accepted. The returned pointer must be freed using
 *                      RTStrFree().
 * @param   ppszRev     Receives pointer of allocated revision string. NULL is
 *                      accepted. The returned pointer must be freed using
 *                      RTStrFree().
 */
VBGLR3DECL(int) VbglR3GetAdditionsVersion(char **ppszVer, char **ppszVerExt, char **ppszRev)
{
    /*
     * Zap the return value up front.
     */
    if (ppszVer)
        *ppszVer   = NULL;
    if (ppszVerExt)
        *ppszVerExt = NULL;
    if (ppszRev)
        *ppszRev   = NULL;


#ifdef RT_OS_WINDOWS
    HKEY hKey;
    int rc = vbglR3QueryAdditionsWinStoragePath(&hKey);
    if (RT_SUCCESS(rc))
    {
        /*
         * Version.
         */
        char szTemp[32];
        if (ppszVer)
        {
            rc = vbglR3QueryRegistryString(hKey, "Version", szTemp, sizeof(szTemp));
            if (RT_SUCCESS(rc))
                rc = RTStrDupEx(ppszVer, szTemp);
        }

        if (   RT_SUCCESS(rc)
            && ppszVerExt)
        {
            rc = vbglR3QueryRegistryString(hKey, "VersionExt", szTemp, sizeof(szTemp));
            if (RT_SUCCESS(rc))
                rc = RTStrDupEx(ppszVerExt, szTemp);
        }

        /*
         * Revision.
         */
        if (   RT_SUCCESS(rc)
            && ppszRev)
        {
            rc = vbglR3QueryRegistryString(hKey, "Revision", szTemp, sizeof(szTemp));
            if (RT_SUCCESS(rc))
                rc = RTStrDupEx(ppszRev, szTemp);
        }

        int rc2 = vbglR3CloseAdditionsWinStoragePath(hKey);
        if (RT_SUCCESS(rc))
            rc = rc2;
        else
        {
            /* Clean up allocated strings on error. */
            if (*ppszVer)
                RTStrFree(*ppszVer);
            if (*ppszVerExt)
                RTStrFree(*ppszVerExt);
            if (*ppszRev)
                RTStrFree(*ppszRev);
        }
    }
    else
    {
        /*
         * No registry entries found, return the version string compiled
         * into this binary.
         */
        rc = vbglR3GetAdditionsCompileTimeVersion(ppszVer, ppszVerExt, ppszRev);
    }
    return rc;

#else  /* !RT_OS_WINDOWS */
    /*
     * On non-Windows platforms just return the compile-time version string.
     */
    return vbglR3GetAdditionsCompileTimeVersion(ppszVer, ppszVerExt, ppszRev);
#endif /* !RT_OS_WINDOWS */
}


/**
 * Retrieves the installation path of Guest Additions.
 *
 * @returns IPRT status value
 * @param   ppszPath    Receives pointer of allocated installation path string.
 *                      The returned pointer must be freed using
 *                      RTStrFree().
 */
VBGLR3DECL(int) VbglR3GetAdditionsInstallationPath(char **ppszPath)
{
    int rc;
#ifdef RT_OS_WINDOWS
    HKEY hKey;
    rc = vbglR3QueryAdditionsWinStoragePath(&hKey);
    if (RT_SUCCESS(rc))
    {
        /* Installation directory. */
        DWORD dwType;
        DWORD dwSize = _MAX_PATH * sizeof(char);
        char *pszTmp = (char*)RTMemAlloc(dwSize + 1);
        if (pszTmp)
        {
            LONG l = RegQueryValueEx(hKey, "InstallDir", NULL, &dwType, (BYTE*)(LPCTSTR)pszTmp, &dwSize);
            if ((l != ERROR_SUCCESS) && (l != ERROR_FILE_NOT_FOUND))
            {
                rc = RTErrConvertFromNtStatus(l);
            }
            else
            {
                if (dwType == REG_SZ)
                    rc = RTStrDupEx(ppszPath, pszTmp);
                else
                    rc = VERR_INVALID_PARAMETER;
                if (RT_SUCCESS(rc))
                {
                    /* Flip slashes. */
                    for (char *pszTmp2 = ppszPath[0]; *pszTmp2; ++pszTmp2)
                        if (*pszTmp2 == '\\')
                            *pszTmp2 = '/';
                }
            }
            RTMemFree(pszTmp);
        }
        else
            rc = VERR_NO_MEMORY;
        rc = vbglR3CloseAdditionsWinStoragePath(hKey);
    }
#else
    /** @todo implement me */
    rc = VERR_NOT_IMPLEMENTED;
#endif
    return rc;
}

