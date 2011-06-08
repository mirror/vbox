/* $Id$ */
/** @file
 * VBoxServiceControlDir - Utility functions for guest directory handling.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/assert.h>
#include <VBox/VBoxGuestLib.h>
#include <VBox/HostServices/GuestControlSvc.h>

#include "VBoxServiceInternal.h"
#include "VBoxServiceUtils.h"

using namespace guestControl;

int VBoxServiceGCtrlDirClose(uint32_t u32ClientId, uint32_t uNumParms)
{
    uint32_t uContextID;
    uint32_t uHandle;

    int rc = VbglR3GuestCtrlGetCmdDirClose(u32ClientId, uNumParms,
                                           &uContextID, &uHandle);
    if (RT_SUCCESS(rc))
    {

    }
    else
        VBoxServiceError(" VBoxServiceGCtrlDir: Failed to retrieve close command! Error: %Rrc\n", rc);
    VBoxServiceVerbose(3, " VBoxServiceGCtrlDir: VBoxServiceGCtrlDirClose returned with %Rrc\n", rc);
    return rc;
}

int VBoxServiceGCtrlDirOpen(uint32_t u32ClientId, uint32_t uNumParms)
{
    uint32_t uContextID;
    char szDir[_1K];
    char szFilter[_1K];
    uint32_t uFlags;
    char szUser[128];
    char szPassword[128];

    int rc = VbglR3GuestCtrlGetCmdDirOpen(u32ClientId, uNumParms,
                                          &uContextID,
                                          szDir, sizeof(szDir),
                                          szFilter, sizeof(szFilter),
                                          &uFlags,
                                          szUser, sizeof(szUser),
                                          szPassword, sizeof(szPassword));
    if (RT_SUCCESS(rc))
    {

    }
    else
        VBoxServiceError(" VBoxServiceGCtrlDir: Failed to retrieve open command! Error: %Rrc\n", rc);
    VBoxServiceVerbose(3, " VBoxServiceGCtrlDir: VBoxServiceGCtrlDirOpen returned with %Rrc\n", rc);
    return rc;
}

int VBoxServiceGCtrlDirRead(uint32_t u32ClientId, uint32_t uNumParms)
{
    uint32_t uContextID;
    uint32_t uHandle;

    int rc = VbglR3GuestCtrlGetCmdDirRead(u32ClientId, uNumParms,
                                          &uContextID, &uHandle);
    if (RT_SUCCESS(rc))
    {

    }
    else
        VBoxServiceError(" VBoxServiceGCtrlDir: Failed to retrieve read command! Error: %Rrc\n", rc);
    VBoxServiceVerbose(3, " VBoxServiceGCtrlDir: VBoxServiceGCtrlDirRead returned with %Rrc\n", rc);
    return rc;
}

