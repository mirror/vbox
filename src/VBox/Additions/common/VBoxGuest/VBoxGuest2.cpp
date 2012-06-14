/* $Id$ */
/** @file
 * VBoxGuest - Guest Additions Driver, bits shared with the windows code.
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
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
#include <iprt/string.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/VBoxGuestLib.h>
#include <VBox/version.h>
#if defined(RT_OS_LINUX) || defined(RT_OS_FREEBSD)
# include "revision-generated.h"
#endif
#include "VBoxGuest2.h"

/** @todo Remove and merge this file with VBoxGuest.cpp when the Windows driver
  *       also will be built from the common sources. */

/**
 * Report the guest information to the host.
 *
 * @returns IPRT status code.
 * @param   enmOSType       The OS type to report.
 */
int VBoxGuestReportGuestInfo(VBOXOSTYPE enmOSType)
{
    /*
     * Allocate and fill in the two guest info reports.
     */
    VMMDevReportGuestInfo2 *pReqInfo2 = NULL;
    VMMDevReportGuestInfo  *pReqInfo1 = NULL;
    int rc = VbglGRAlloc((VMMDevRequestHeader **)&pReqInfo2, sizeof (VMMDevReportGuestInfo2), VMMDevReq_ReportGuestInfo2);
    Log(("VBoxGuestReportGuestInfo: VbglGRAlloc VMMDevReportGuestInfo2 completed with rc=%Rrc\n", rc));
    if (RT_SUCCESS(rc))
    {
        pReqInfo2->guestInfo.additionsMajor    = VBOX_VERSION_MAJOR;
        pReqInfo2->guestInfo.additionsMinor    = VBOX_VERSION_MINOR;
        pReqInfo2->guestInfo.additionsBuild    = VBOX_VERSION_BUILD;
        pReqInfo2->guestInfo.additionsRevision = VBOX_SVN_REV;
        pReqInfo2->guestInfo.additionsFeatures = 0; /* (no features defined yet) */
        RTStrCopy(pReqInfo2->guestInfo.szName, sizeof(pReqInfo2->guestInfo.szName), VBOX_VERSION_STRING);

        rc = VbglGRAlloc((VMMDevRequestHeader **)&pReqInfo1, sizeof (VMMDevReportGuestInfo), VMMDevReq_ReportGuestInfo);
        Log(("VBoxGuestReportGuestInfo: VbglGRAlloc VMMDevReportGuestInfo completed with rc=%Rrc\n", rc));
        if (RT_SUCCESS(rc))
        {
            pReqInfo1->guestInfo.interfaceVersion = VMMDEV_VERSION;
            pReqInfo1->guestInfo.osType           = enmOSType;

            /*
             * There are two protocols here:
             *      1. Info2 + Info1. Supported by >=3.2.51.
             *      2. Info1 and optionally Info2. The old protocol.
             *
             * We try protocol 1 first.  It will fail with VERR_NOT_SUPPORTED
             * if not supported by the VMMDev (message ordering requirement).
             */
            rc = VbglGRPerform(&pReqInfo2->header);
            Log(("VBoxGuestReportGuestInfo: VbglGRPerform VMMDevReportGuestInfo2 completed with rc=%Rrc\n", rc));
            if (RT_SUCCESS(rc))
            {
                rc = VbglGRPerform(&pReqInfo1->header);
                Log(("VBoxGuestReportGuestInfo: VbglGRPerform VMMDevReportGuestInfo completed with rc=%Rrc\n", rc));
            }
            else if (   rc == VERR_NOT_SUPPORTED
                     || rc == VERR_NOT_IMPLEMENTED)
            {
                rc = VbglGRPerform(&pReqInfo1->header);
                Log(("VBoxGuestReportGuestInfo: VbglGRPerform VMMDevReportGuestInfo completed with rc=%Rrc\n", rc));
                if (RT_SUCCESS(rc))
                {
                    rc = VbglGRPerform(&pReqInfo2->header);
                    Log(("VBoxGuestReportGuestInfo: VbglGRPerform VMMDevReportGuestInfo2 completed with rc=%Rrc\n", rc));
                    if (rc == VERR_NOT_IMPLEMENTED)
                        rc = VINF_SUCCESS;
                }
            }
            VbglGRFree(&pReqInfo1->header);
        }
        VbglGRFree(&pReqInfo2->header);
    }

    return rc;
}


/**
 * Report the guest driver status to the host.
 *
 * @returns IPRT status code.
 * @param   fActive         Flag whether the driver is now active or not.
 */
int VBoxGuestReportDriverStatus(bool fActive)
{
    /*
     * Report guest status of the VBox driver to the host.
     */
    VMMDevReportGuestStatus *pReq2 = NULL;
    int rc = VbglGRAlloc((VMMDevRequestHeader **)&pReq2, sizeof(*pReq2), VMMDevReq_ReportGuestStatus);
    Log(("VBoxGuestReportDriverStatus: VbglGRAlloc VMMDevReportGuestStatus completed with rc=%Rrc\n", rc));
    if (RT_SUCCESS(rc))
    {
        pReq2->guestStatus.facility = VBoxGuestFacilityType_VBoxGuestDriver;
        pReq2->guestStatus.status = fActive ?
                                    VBoxGuestFacilityStatus_Active
                                  : VBoxGuestFacilityStatus_Inactive;
        pReq2->guestStatus.flags = 0;
        rc = VbglGRPerform(&pReq2->header);
        Log(("VBoxGuestReportDriverStatus: VbglGRPerform VMMDevReportGuestStatus completed with fActive=%d, rc=%Rrc\n",
             fActive ? 1 : 0, rc));
        if (rc == VERR_NOT_IMPLEMENTED) /* Compatibility with older hosts. */
            rc = VINF_SUCCESS;
        VbglGRFree(&pReq2->header);
    }

    return rc;
}

