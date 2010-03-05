/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, Additions Info.
 */

/*
 * Copyright (C) 2007-2010 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
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
 * Fallback for vbglR3GetAdditionsVersion.
 */
static int vbglR3GetAdditionsCompileTimeVersion(char **ppszVer, char **ppszRev)
{
    if (ppszVer)
    {
        *ppszVer = RTStrDup(VBOX_VERSION_STRING);
        if (!*ppszVer)
            return VERR_NO_STR_MEMORY;
    }

    if (ppszRev)
    {
        char szRev[64];
        RTStrPrintf(szRev, sizeof(szRev), "%d", VBOX_SVN_REV);
        *ppszRev = RTStrDup(szRev);
        if (!*ppszRev)
        {
            if (ppszVer)
            {
                RTStrFree(*ppszVer);
                *ppszVer = NULL;
            }
            return VERR_NO_STR_MEMORY;
        }
    }

    return VINF_SUCCESS;
}

#ifdef RT_OS_WINDOWS
/**
 * Looks up the storage path handle (registry).
 *
 * @returns IPRT status value
 * @param   hKey        Receives pointer of allocated version string. NULL is
 *                      accepted. The returned pointer must be closed by
 *                      vbglR3CloseAdditionsWinStoragePath().
 */
static int vbglR3GetAdditionsWinStoragePath(PHKEY phKey)
{
    /*
     * Try get the *installed* version first.
     */
    LONG r;

    /* Check the new path first. */
    r = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Sun\\VirtualBox Guest Additions", 0, KEY_READ, phKey);
# ifdef RT_ARCH_AMD64
    if (r != ERROR_SUCCESS)
    {
        /* Check Wow6432Node (for new entries). */
        r = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Wow6432Node\\Sun\\VirtualBox Guest Additions", 0, KEY_READ, phKey);
    }
# endif

    /* Still no luck? Then try the old xVM paths ... */
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
 *                      vbglR3GetAdditionsWinStoragePath().
 */
static int vbglR3CloseAdditionsWinStoragePath(HKEY hKey)
{
    return RTErrConvertFromWin32(RegCloseKey(hKey));
}

#endif /* RT_OS_WINDOWS */

/**
 * Retrieves the installed Guest Additions version and/or revision.
 *
 * @returns IPRT status value
 * @param   ppszVer     Receives pointer of allocated version string. NULL is
 *                      accepted. The returned pointer must be freed using
 *                      RTStrFree().
 * @param   ppszRev     Receives pointer of allocated revision string. NULL is
 *                      accepted. The returned pointer must be freed using
 *                      RTStrFree().
 */
VBGLR3DECL(int) VbglR3GetAdditionsVersion(char **ppszVer, char **ppszRev)
{
#ifdef RT_OS_WINDOWS
    HKEY hKey;
    int rc = vbglR3GetAdditionsWinStoragePath(&hKey);
    if (RT_SUCCESS(rc))
    {
        /* Version. */
        LONG l;
        DWORD dwType;
        DWORD dwSize = 32;
        char *pszTmp;
        if (ppszVer)
        {
            pszTmp = (char*)RTMemAlloc(dwSize);
            if (pszTmp)
            {
                l = RegQueryValueEx(hKey, "Version", NULL, &dwType, (BYTE*)(LPCTSTR)pszTmp, &dwSize);
                if (l == ERROR_SUCCESS)
                {
                    if (dwType == REG_SZ)
                        rc = RTStrDupEx(ppszVer, pszTmp);
                    else
                        rc = VERR_INVALID_PARAMETER;
                }
                else
                {
                    rc = RTErrConvertFromWin32(l);
                }
                RTMemFree(pszTmp);
            }
            else
                rc = VERR_NO_MEMORY;
        }
        /* Revision. */
        if (ppszRev)
        {
            dwSize = 32; /* Reset */
            pszTmp = (char*)RTMemAlloc(dwSize);
            if (pszTmp)
            {
                l = RegQueryValueEx(hKey, "Revision", NULL, &dwType, (BYTE*)(LPCTSTR)pszTmp, &dwSize);
                if (l == ERROR_SUCCESS)
                {
                    if (dwType == REG_SZ)
                        rc = RTStrDupEx(ppszRev, pszTmp);
                    else
                        rc = VERR_INVALID_PARAMETER;
                }
                else
                {
                    rc = RTErrConvertFromWin32(l);
                }
                RTMemFree(pszTmp);
            }
            else
                rc = VERR_NO_MEMORY;

            if (RT_FAILURE(rc) && ppszVer)
            {
                RTStrFree(*ppszVer);
                *ppszVer = NULL;
            }
        }
        rc = vbglR3CloseAdditionsWinStoragePath(hKey);
    }
    else
    {
        /*
         * No registry entries found, return the version string compiled
         * into this binary.
         */
        rc = vbglR3GetAdditionsCompileTimeVersion(ppszVer, ppszRev);
    }
    return rc;

#else  /* !RT_OS_WINDOWS */
    /*
     * On non-Windows platforms just return the compile-time version string.
     */
    return vbglR3GetAdditionsCompileTimeVersion(ppszVer, ppszRev);
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
    rc = vbglR3GetAdditionsWinStoragePath(&hKey);
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

