
/* $Id$ */
/** @file
 * VBoxHostVersion - Checks the host's VirtualBox version and notifies
 *                   the user in case of an update.
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

#include "VBoxHostVersion.h"
#include "VBoxTray.h"
#include "helpers.h"

#include <VBox/VBoxGuestLib.h>

/* Returns 0 if equal, 1 if Ver1 is greater, 2 if Ver2 is greater
 * Requires strings in form of "majorVer.minorVer.build" */
int VBoxCompareVersion (const char* pszVer1, const char* pszVer2)
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

int VBoxCheckHostVersion ()
{
    int rc;
    uint32_t uGuestPropSvcClientID;

    rc = VbglR3GuestPropConnect(&uGuestPropSvcClientID);
    if (RT_SUCCESS(rc))
        Log(("VBoxTray: Property Service Client ID: %ld\n", uGuestPropSvcClientID));
    else
        Log(("VBoxTray: Failed to connect to the guest property service! Error: %d\n", rc));

    char szMsg[256] = "\0"; /* Sizes according to MSDN. */
    char szTitle[64] = "\0";

    if (RT_SUCCESS(rc))
    {
        /* Look up host version (revision) */
        char *pszVBoxHostVer; // , *pszVBoxHostRev;
        rc = VbglR3GuestPropReadValueAlloc(uGuestPropSvcClientID, "/VirtualBox/HostInfo/VBoxVer", &pszVBoxHostVer);
        if(RT_FAILURE(rc))
            Log(("VBoxTray: Could not read VBox host version! rc = %d\n", rc));

        /*rc = VbglR3GuestPropReadValueAlloc(uGuestPropSvcClientID, "/VirtualBox/HostInfo/VBoxRev", &pszVBoxHostRev);
        if(RT_FAILURE(rc))
            Log(("VBoxTray: Could not read VBox host revision! rc = %d\n", rc));*/

        if (RT_SUCCESS(rc))
        {
            Log(("VBoxTray: Host version: %s\n", pszVBoxHostVer));

            /* Look up guest version */
            char szVBoxGuestVer[32];
            char szVBoxGuestRev[32];

            rc = getAdditionsVersion(szVBoxGuestVer, sizeof(szVBoxGuestVer), szVBoxGuestRev, sizeof(szVBoxGuestRev));
            if (RT_SUCCESS(rc))
            {
                Log(("VBoxTray: Additions version: %s\n", szVBoxGuestVer));

                /* Look up last informed host version */
                char szVBoxHostVerLastChecked[32];
                HKEY hKey;
                LONG lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Sun\\VirtualBox Guest Additions\\VBoxTray", 0, KEY_READ, &hKey);
                if (lRet == ERROR_SUCCESS)
                {
                    DWORD dwType;
                    DWORD dwSize = sizeof(szVBoxHostVerLastChecked);
                    lRet = RegQueryValueEx(hKey, "HostVerLastChecked", NULL, &dwType, (BYTE*)szVBoxHostVerLastChecked, &dwSize);
                    if (lRet != ERROR_SUCCESS && lRet != ERROR_FILE_NOT_FOUND)
                        Log(("VBoxTray: Could not read HostVerLastChecked! Error = %ld\n", lRet));
                    RegCloseKey(hKey);
                }
                else if (lRet != ERROR_FILE_NOT_FOUND)
                {
                    Log(("VBoxTray: Could not open VBoxTray registry key! Error = %ld\n", lRet));
                    rc = RTErrConvertFromWin32(lRet);
                }

                /** @todo implement a special value of last informed host version (or a new flag) to disable this service. */

                /* Compare both versions and prepare message */
                if (   RT_SUCCESS(rc)
                    && strcmp(pszVBoxHostVer, szVBoxHostVerLastChecked) != 0        /* Make sure we did not process this host version already */
                    && VBoxCompareVersion(pszVBoxHostVer, szVBoxGuestVer) == 1)     /* Is host version greater than guest add version? */
                {
                    /** @todo add some translation macros here */
                    _snprintf(szTitle, sizeof(szTitle), "New VirtualBox Guest Additions %s available!", pszVBoxHostVer);
                    _snprintf(szMsg, sizeof(szMsg), "A new version %s of VirtualBox has been installed on the host. "
                                                    "To ensure that this guest can take advantage of the host's version, "
                                                    "please also update the Guest Additions (current: %s) of this machine.",
                                                    pszVBoxHostVer, szVBoxGuestVer);

                    /* Save the version to just do a balloon once per new version */
                    if (RT_SUCCESS(rc))
                    {
                        lRet = RegCreateKeyEx (HKEY_LOCAL_MACHINE,
                                               "SOFTWARE\\Sun\\VirtualBox Guest Additions\\VBoxTray",
                                               0,           /* Reserved */
                                               NULL,        /* lpClass [in, optional] */
                                               0,           /* dwOptions [in] */
                                               KEY_WRITE,
                                               NULL,        /* lpSecurityAttributes [in, optional] */
                                               &hKey,
                                               NULL);       /* lpdwDisposition [out, optional] */
                        if (lRet == ERROR_SUCCESS)
                        {
                            lRet = RegSetValueEx(hKey, "HostVerLastChecked", 0, REG_SZ, (BYTE*)pszVBoxHostVer, strlen(pszVBoxHostVer)*sizeof(char));
                            if (lRet != ERROR_SUCCESS)
                                Log(("VBoxTray: Could not write HostVerLastChecked! Error = %ld\n", lRet));
                            RegCloseKey(hKey);
                        }
                        else
                            Log(("VBoxTray: Could not open VBoxTray registry key! Error = %ld\n", lRet));

                        if(lRet != ERROR_SUCCESS)
                            rc = RTErrConvertFromWin32(lRet);
                    }
                }
            }
        }
        VbglR3GuestPropReadValueFree(pszVBoxHostVer);
        //VbglR3GuestPropReadValueFree(pszVBoxHostRev);
    }

    if(RT_SUCCESS(rc) && strlen(szMsg))
    {
        rc = showBalloonTip(gInstance, gToolWindow, ID_TRAYICON, szMsg, szTitle, 5000, 0);
        if(RT_FAILURE(rc))
            Log(("VBoxTray: Could not show version notifier balloon tooltip! rc = %d\n", rc));
    }

    VbglR3GuestPropDisconnect(uGuestPropSvcClientID);
    return rc;
}

