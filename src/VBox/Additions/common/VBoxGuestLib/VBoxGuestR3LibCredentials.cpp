/* $Id: */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, user credentials.
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
#include <iprt/string.h>
#include <VBox/log.h>

#include "VBGLR3Internal.h"


/**
 * Checks whether user credentials are available to the guest or not.
 *
 * @returns true if credentials are available, false if not (or error occured).
 *
 */
VBGLR3DECL(bool) VbglR3CredentialsAreAvailable(void)
{
    int rc;
    VMMDevCredentials vmmreqCredentials;

    memset(&vmmreqCredentials, 0, sizeof(vmmreqCredentials));

    vmmdevInitRequest((VMMDevRequestHeader*)&vmmreqCredentials, VMMDevReq_QueryCredentials);
    vmmreqCredentials.u32Flags |= VMMDEV_CREDENTIALS_QUERYPRESENCE;
    rc = vbglR3GRPerform(&vmmreqCredentials.header);

    bool fAvailable = false;
    if (   RT_SUCCESS(rc)
        && ((vmmreqCredentials.u32Flags & VMMDEV_CREDENTIALS_PRESENT) != 0))
    {
        fAvailable = true;
    }
    return fAvailable;
}


/**
 * Retrieves user credentials to log into a guest OS.
 *
 * @returns IPRT status value
 * @param   ppszUser        Receives pointer of allocated user name string. NULL is accepted.
 *                          The returned pointer must be freed using RTStrFree().
 * @param   ppszPassword    Receives pointer of allocated user password string. NULL is accepted.
 *                          The returned pointer must be freed using RTStrFree().
 * @param   ppszDomain      Receives pointer of allocated domain name string. NULL is accepted.
 *                          The returned pointer must be freed using RTStrFree().
 */
VBGLR3DECL(int) VbglR3CredentialsRetrieve(char **ppszUser, char **ppszPassword, char **ppszDomain)
{
    int rc;
    VMMDevCredentials vmmreqCredentials;

    memset(&vmmreqCredentials, 0, sizeof(vmmreqCredentials));

    vmmdevInitRequest((VMMDevRequestHeader*)&vmmreqCredentials, VMMDevReq_QueryCredentials);
    vmmreqCredentials.u32Flags |= VMMDEV_CREDENTIALS_READ;
    vmmreqCredentials.u32Flags |= VMMDEV_CREDENTIALS_CLEAR;
    rc = vbglR3GRPerform(&vmmreqCredentials.header);

    if (RT_SUCCESS(rc))
    {
        /** @todo error checking */
        rc = RTStrDupEx(ppszUser, vmmreqCredentials.szUserName);
        rc = RTStrDupEx(ppszPassword, vmmreqCredentials.szPassword);
        rc = RTStrDupEx(ppszDomain, vmmreqCredentials.szDomain);
    }
    return rc;
}

