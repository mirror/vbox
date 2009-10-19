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
 *  Requires strings in form of "majorVer.minorVer.build".
 *
 * @returns 0 if equal, 1 if Ver1 is greater, 2 if Ver2 is greater.
 *
 * @param   pszVer1     First version string to compare.
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


/** Checks for a Guest Additions update by comparing the installed version on
 *  the guest and the reported host version.
 *
 * @returns VBox status code
 *
 * @param   u32ClientId          The client id returned by VbglR3InfoSvcConnect().
 * @param   bUpdate              Receives pointer to boolean flag indicating whether 
                                 an update was found or not.
 * @param   ppszHostVersion      Receives pointer of allocated version string.
 *                               The returned pointer must be freed using RTStrFree().
 * @param   ppszGuestVersion     Receives pointer of allocated revision string.
 *                               The returned pointer must be freed using RTStrFree().
 */
VBGLR3DECL(int) VbglR3HostVersionCheckForUpdate(uint32_t u32ClientId, bool *bUpdate, char **ppszHostVersion, char **ppszGuestVersion)
{
    int rc;

    Assert(u32ClientId > 0);
    Assert(bUpdate);
    Assert(ppszHostVersion);
    Assert(ppszGuestVersion);

    /* We assume we have an update initially. 
       Every block down below is allowed to veto */
    *bUpdate = true;

    /* Do we need to do all this stuff? */
    char *pszCheckHostVersion;
    rc = VbglR3GuestPropReadValueAlloc(u32ClientId, "/VirtualBox/GuestAdd/CheckHostVersion", &pszCheckHostVersion);
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
            LogRel(("No host version update check performed (disabled)."));
            *bUpdate = false;
        }
        VbglR3GuestPropReadValueFree(pszCheckHostVersion);
    }    

    /* Make sure we only notify the user once by comparing the host version with
     * the last checked host version (if any) */
    if (RT_SUCCESS(rc) && *bUpdate)
    {
        /* Look up host version */
        rc = VbglR3GuestPropReadValueAlloc(u32ClientId, "/VirtualBox/HostInfo/VBoxVer", ppszHostVersion);
        if (RT_FAILURE(rc))
        {
            LogFlow(("Could not read VBox host version! rc = %d\n", rc));
        }
        else
        {
            LogFlow(("Host version: %s\n", *ppszHostVersion));

            /* Get last checked host version */
            char *pszLastCheckedHostVersion;
            rc = VbglR3HostVersionLastCheckedLoad(u32ClientId, &pszLastCheckedHostVersion);
            if (RT_SUCCESS(rc))
            {
                LogFlow(("Last checked host version: %s\n", pszLastCheckedHostVersion));
                if (strcmp(*ppszHostVersion, pszLastCheckedHostVersion) == 0)
                    *bUpdate = false; /* We already notified this version, skip */
                VbglR3GuestPropReadValueFree(pszLastCheckedHostVersion);
            }
            else if (rc == VERR_NOT_FOUND) /* Never wrote a last checked host version before */
            {
                LogFlow(("Never checked a host version before."));
                rc = VINF_SUCCESS;
            }
        }

        if (RT_SUCCESS(rc))
        {
            /* Look up guest version */
            rc = VbglR3GetAdditionsVersion(ppszGuestVersion, NULL /* Revision not needed here */);
        }
    }
     
    /* Do the actual version comparison (if needed, see block(s) above) */
    if (RT_SUCCESS(rc) && *bUpdate)
    {
        if (VbglR3HostVersionCompare(*ppszHostVersion, *ppszGuestVersion) == 1) /* Is host version greater than guest add version? */
        {
            /* Yay, we have an update! */
            LogRel(("Guest Additions update found! Please upgrade this machine to the latest Guest Additions."));
        }
        else
        {
            /* How sad ... */
            *bUpdate = false;
        }
    }

    /* Cleanup on failure */
    if (RT_FAILURE(rc))
    {
        VbglR3GuestPropReadValueFree(*ppszHostVersion);
        VbglR3GuestPropReadValueFree(*ppszGuestVersion);
    }
    return rc;
}


/** Retrieves the last checked host version.
 *
 * @returns VBox status code.
 *
 * @param   u32ClientId     The client id returned by VbglR3InfoSvcConnect().
 * @param   ppszVer         Receives pointer of allocated version string.
 *                          The returned pointer must be freed using RTStrFree() on VINF_SUCCESS.
 */
VBGLR3DECL(int) VbglR3HostVersionLastCheckedLoad(uint32_t u32ClientId, char **ppszVer)
{
    Assert(u32ClientId > 0);
    Assert(ppszVer);
    return VbglR3GuestPropReadValueAlloc(u32ClientId, "/VirtualBox/GuestAdd/HostVerLastChecked", ppszVer);
}


/** Stores the last checked host version for later lookup.
 *  Requires strings in form of "majorVer.minorVer.build".
 *
 * @returns VBox status code.
 *
 * @param   u32ClientId     The client id returned by VbglR3InfoSvcConnect().
 * @param   pszVer          Pointer to version string to store.
 */
VBGLR3DECL(int) VbglR3HostVersionLastCheckedStore(uint32_t u32ClientId, const char *pszVer)
{
    Assert(u32ClientId > 0);
    Assert(pszVer);  
    return VbglR3GuestPropWriteValue(u32ClientId, "/VirtualBox/GuestAdd/HostVerLastChecked", pszVer);
}
