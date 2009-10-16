/* $Id: */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, host version check.
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
#include <stdlib.h>
#include <stdio.h>
#include <iprt/string.h>
#include <VBox/log.h>

#ifdef RT_OS_WINDOWS
 #define WIN32_LEAN_AND_MEAN
 #include <windows.h>
#endif

#include "VBGLR3Internal.h"

/** Compares two VirtualBox version strings and returns the result.
 *  Requires strings in form of "majorVer.minorVer.build"
 *
 * @returns 0 if equal, 1 if Ver1 is greater, 2 if Ver2 is greater.
 *
 * @param   pszVer1		First version string to compare.
 * @param   pszVer2     First version string to compare.
 *
 */
VBGLR3DECL(int) VbglR3HostVersionCompare(const char *pszVer1, const char *pszVer2)
{
    int rc = 0;
    int iVer1Major, iVer1Minor, iVer1Build;
    sscanf(pszVer1, "%d.%d.%d", &iVer1Major, &iVer1Minor, &iVer1Build);
    int iVer2Major, iVer2Minor, iVer2Build;
    sscanf(pszVer2, "%d.%d.%d", &iVer2Major, &iVer2Minor, &iVer2Build);

    int iVer1Final = (iVer1Major * 10000) + (iVer1Minor * 100) + iVer1Build;
    int iVer2Final = (iVer2Major * 10000) + (iVer2Minor * 100) + iVer2Build;

    if (iVer1Final > iVer2Final)
        rc = 1;
    else if (iVer2Final > iVer1Final)
        rc = 2;
    return rc;
}


/** @todo Docs */
VBGLR3DECL(bool) VbglR3HostVersionCheckForUpdate(char **ppszHostVersion, char **ppszGuestVersion)
{
    int rc;
    uint32_t uGuestPropSvcClientID;

    rc = VbglR3GuestPropConnect(&uGuestPropSvcClientID);
    if (RT_SUCCESS(rc))
        LogFlow(("Property Service Client ID: %ld\n", uGuestPropSvcClientID));
    else
        LogFlow(("Failed to connect to the guest property service! Error: %d\n", rc));

    /* Do we need to do all this stuff? */
    char *pszCheckHostVersion;
    rc = VbglR3GuestPropReadValueAlloc(uGuestPropSvcClientID, "/VirtualBox/GuestAdd/CheckHostVersion", &pszCheckHostVersion);
    if (RT_FAILURE(rc))
    {
        if (rc == VERR_NOT_FOUND)
            rc = VINF_SUCCESS; /* If we don't find the value above we do the check by default */
        else
            LogFlow(("Could not read check host version flag! rc = %d\n", rc));
    }
    else
    {
        /* Only don't do the check if we have a valid "0" in it */
        if (   atoi(pszCheckHostVersion) == 0
            && strlen(pszCheckHostVersion))
        {
            rc = VERR_NOT_SUPPORTED;
        }
        VbglR3GuestPropReadValueFree(pszCheckHostVersion);
    }
    if (rc == VERR_NOT_SUPPORTED)
        LogRel(("No host version check performed (disabled)."));

    if (RT_SUCCESS(rc))
    {
        /* Look up host version (revision) */
        rc = VbglR3GuestPropReadValueAlloc(uGuestPropSvcClientID, "/VirtualBox/HostInfo/VBoxVer", ppszHostVersion);
        if (RT_FAILURE(rc))
        {
            LogFlow(("Could not read VBox host version! rc = %d\n", rc));
        }
        else
        {
            LogFlow(("Host version: %s\n", *ppszHostVersion));

            /* Look up guest version */
            rc = VbglR3GetAdditionsVersion(ppszGuestVersion, NULL /* Revision */);
            if (RT_SUCCESS(rc))
            {
                LogFlow(("Additions version: %s\n", *ppszGuestVersion));

                /* Look up last informed host version */
                char szVBoxHostVerLastChecked[32];
#ifdef RT_OS_WINDOWS
                HKEY hKey;
                LONG lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Sun\\VirtualBox Guest Additions", 0, KEY_READ, &hKey);
                if (lRet == ERROR_SUCCESS)
                {
                    DWORD dwType;
                    DWORD dwSize = sizeof(szVBoxHostVerLastChecked);
                    lRet = RegQueryValueEx(hKey, "HostVerLastChecked", NULL, &dwType, (BYTE*)szVBoxHostVerLastChecked, &dwSize);
                    if (lRet != ERROR_SUCCESS && lRet != ERROR_FILE_NOT_FOUND)
                        LogFlow(("Could not read HostVerLastChecked! Error = %ld\n", lRet));
                    RegCloseKey(hKey);
                }
                else if (lRet != ERROR_FILE_NOT_FOUND)
                {
                    LogFlow(("Could not open registry key! Error = %ld\n", lRet));
                    rc = RTErrConvertFromWin32(lRet);
                }
#else

#endif
                /* Compare both versions and prepare message */
                if (   RT_SUCCESS(rc)
                    && strcmp(*ppszHostVersion, szVBoxHostVerLastChecked) != 0        /* Make sure we did not process this host version already */
                    && VbglR3HostVersionCompare(*ppszHostVersion, *ppszGuestVersion) == 1)     /* Is host version greater than guest add version? */
                {
                    LogFlow(("Update found."));
                }
		else rc = VERR_VERSION_MISMATCH; /* No update found. */
            }
        }
    }

    if (RT_FAILURE(rc))
    {
	if (*ppszHostVersion)
	    VbglR3GuestPropReadValueFree(*ppszHostVersion);
	if (*ppszGuestVersion)
	    VbglR3GuestPropReadValueFree(*ppszGuestVersion);
    }
    return rc == VINF_SUCCESS ? true : false;
}


/** @todo Docs */
VBGLR3DECL(int) VbglR3HostVersionStore(const char* pszVer)
{
    int rc;
    uint32_t uGuestPropSvcClientID;

    rc = VbglR3GuestPropConnect(&uGuestPropSvcClientID);
    if (RT_FAILURE(rc))
    {
    LogFlow(("Failed to connect to the guest property service! Error: %d\n", rc));
    }
    else
    {
#ifdef RT_OS_WINDOWS
	HKEY hKey;
	long lRet;
	lRet = RegCreateKeyEx (HKEY_LOCAL_MACHINE,
						   "SOFTWARE\\Sun\\VirtualBox Guest Additions",
						   0,           /* Reserved */
						   NULL,        /* lpClass [in, optional] */
						   0,           /* dwOptions [in] */
						   KEY_WRITE,
						   NULL,        /* lpSecurityAttributes [in, optional] */
						   &hKey,
						   NULL);       /* lpdwDisposition [out, optional] */
	if (lRet == ERROR_SUCCESS)
	{
	    lRet = RegSetValueEx(hKey, "HostVerLastChecked", 0, REG_SZ, (BYTE*)pszVBoxHostVer, (DWORD)(strlen(pszVBoxHostVer)*sizeof(char)));
	    if (lRet != ERROR_SUCCESS)
		    LogFlow(("Could not write HostVerLastChecked! Error = %ld\n", lRet));
	    RegCloseKey(hKey);
	}
	else
	    LogFlow(("Could not open registry key! Error = %ld\n", lRet));

	if (lRet != ERROR_SUCCESS)
	    rc = RTErrConvertFromWin32(lRet);
#else
#endif
    }
    return rc;
}
