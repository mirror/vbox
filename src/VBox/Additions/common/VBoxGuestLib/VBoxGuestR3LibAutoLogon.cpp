/* $Id */
/** @file
 * VBoxGuestR3LibAutoLogon - Ring-3 utility functions for auto-logon modules
 * (VBoxGINA / VBoxCredProv / pam_vbox).
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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
#include <iprt/asm.h>
#include <iprt/mem.h>
#include <iprt/rand.h>
#include <iprt/string.h>
#include <VBox/log.h>

#include "VBGLR3Internal.h"


/**
 * Reports the current auto-logon status to the host.
 *
 * This makes sure that the Failed state is sticky.
 *
 * @return  IPRT status code.
 * @param   enmStatus               Status to report to the host.
 */
VBGLR3DECL(int) VbglR3AutoLogonReportStatus(VBoxGuestFacilityStatus enmStatus)
{
    /*
     * VBoxGuestFacilityStatus_Failed is sticky.
     */
    static VBoxGuestFacilityStatus s_enmLastStatus = VBoxGuestFacilityStatus_Inactive;
    if (s_enmLastStatus != VBoxGuestFacilityStatus_Failed)
    {
        int rc = VbglR3ReportAdditionsStatus(VBoxGuestFacilityType_AutoLogon,
                                             enmStatus, 0 /* Flags */);
        if (rc == VERR_NOT_SUPPORTED)
        {
            /*
             * To maintain backwards compatibility to older hosts which don't have
             * VMMDevReportGuestStatus implemented we set the appropriate status via
             * guest property to have at least something.
             */
            uint32_t u32ClientId = 0;
            rc = VbglR3GuestPropConnect(&u32ClientId);
            if (RT_SUCCESS(rc))
            {
                /** @todo Move VBoxGuestStatusCurrent -> const char* to an own function. */
                char szStatus[RTPATH_MAX];
                size_t cbRet = 0;
                switch (enmStatus)
                {
                    case VBoxGuestFacilityStatus_Inactive:
                        cbRet = RTStrPrintf(szStatus, sizeof(szStatus), "Inactive");
                        break;
                    case VBoxGuestFacilityStatus_Paused:
                        cbRet = RTStrPrintf(szStatus, sizeof(szStatus), "Disabled");
                        break;
                    case VBoxGuestFacilityStatus_PreInit:
                        cbRet = RTStrPrintf(szStatus, sizeof(szStatus), "PreInit");
                        break;
                    case VBoxGuestFacilityStatus_Init:
                        cbRet = RTStrPrintf(szStatus, sizeof(szStatus), "Init");
                        break;
                    case VBoxGuestFacilityStatus_Active:
                        cbRet = RTStrPrintf(szStatus, sizeof(szStatus), "Active");
                        break;
                    case VBoxGuestFacilityStatus_Terminating:
                        cbRet = RTStrPrintf(szStatus, sizeof(szStatus), "Terminating");
                        break;
                    case VBoxGuestFacilityStatus_Terminated:
                        cbRet = RTStrPrintf(szStatus, sizeof(szStatus), "Terminated");
                        break;
                    case VBoxGuestFacilityStatus_Failed:
                        cbRet = RTStrPrintf(szStatus, sizeof(szStatus), "Failed");
                        break;
                    default:
                        /* cbRet will be 0. */
                        break;
                }

                if (cbRet)
                {
                    const char szPath[] = "/VirtualBox/GuestInfo/OS/AutoLogonStatus";

                    /*
                     * Because a value can be temporary we have to make sure it also
                     * gets deleted when the property cache did not have the chance to
                     * gracefully clean it up (due to a hard VM reset etc), so set this
                     * guest property using the TRANSRESET flag..
                     */
                    rc = VbglR3GuestPropWrite(u32ClientId, szPath, szStatus, "TRANSRESET");
                    if (rc == VERR_PARSE_ERROR)
                    {
                        /* Host does not support the "TRANSRESET" flag, so only
                         * use the "TRANSIENT" flag -- better than nothing :-). */
                        rc = VbglR3GuestPropWrite(u32ClientId, szPath, szStatus, "TRANSIENT");
                    }
                }
                else
                    rc = VERR_INVALID_PARAMETER;

                VbglR3GuestPropDisconnect(u32ClientId);
            }
        }

        s_enmLastStatus = enmStatus;
    }
    return VINF_SUCCESS;
}


/**
 * Detects whether our process is running in a remote session or not.
 *
 * @return  bool        true if running in a remote session, false if not.
 */
VBGLR3DECL(bool) VbglR3AutoLogonIsRemoteSession(void)
{
#ifdef RT_OS_WINDOWS
    return (0 != GetSystemMetrics(SM_REMOTESESSION)) ? true : false;
#else
    return false; /* Not implemented. */
#endif
}


