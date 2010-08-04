/* $Id$ */
/** @file
 * VBoxGuestR0LibMisc - Miscellaneous functions.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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

#include <iprt/string.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/VBoxGuestLib.h>
#include <VBox/version.h>


DECLR0VBGL(int) VbglR0MiscReportGuestInfo(VBOXOSTYPE enmOSType)
{
    VMMDevReportGuestInfo2 *pReq = NULL;
    int rc = VbglGRAlloc((VMMDevRequestHeader **)&pReq, sizeof (VMMDevReportGuestInfo2), VMMDevReq_ReportGuestInfo2);
    if (RT_SUCCESS(rc))
    {
        pReq->guestInfo.additionsMajor = VBOX_VERSION_MAJOR;
        pReq->guestInfo.additionsMinor = VBOX_VERSION_MINOR;
        pReq->guestInfo.additionsBuild = VBOX_VERSION_BUILD;
        pReq->guestInfo.additionsRevision = VBOX_SVN_REV;
        pReq->guestInfo.additionsFeatures = 0; /* Not (never?) used. */
        RTStrCopy(pReq->guestInfo.szName, sizeof(pReq->guestInfo.szName), VBOX_VERSION_STRING);

        rc = VbglGRPerform(&pReq->header);
        if (rc == VERR_NOT_IMPLEMENTED) /* Compatibility with older hosts. */
            rc = VINF_SUCCESS;
        VbglGRFree(&pReq->header);
    }

    /*
     * Report guest status of the VBox driver to the host.
     */
    if (RT_SUCCESS(rc))
    {
        VMMDevReportGuestStatus *pReq2;
        rc = VbglGRAlloc((VMMDevRequestHeader **)&pReq2, sizeof(*pReq2), VMMDevReq_ReportGuestStatus);
        if (RT_SUCCESS(rc))
        {
            pReq2->guestStatus.facility = VBoxGuestStatusFacility_VBoxGuestDriver;
            pReq2->guestStatus.status = VBoxGuestStatusCurrent_Active; /** @todo Are we actually *really* active at this point? */
            pReq2->guestStatus.flags = 0;
            rc = VbglGRPerform(&pReq2->header);
            if (rc == VERR_NOT_IMPLEMENTED) /* Compatibility with older hosts. */
                rc = VINF_SUCCESS;
            VbglGRFree(&pReq2->header);
        }
    }

    /*
     * VMMDevReportGuestInfo acts as a beacon and signals the host that all guest information is
     * now complete.  So always send this report last!
     */
    if (RT_SUCCESS(rc))
    {
        VMMDevReportGuestInfo *pReq3 = NULL;
        rc = VbglGRAlloc((VMMDevRequestHeader **)&pReq3, sizeof (VMMDevReportGuestInfo), VMMDevReq_ReportGuestInfo);
        if (RT_SUCCESS(rc))
        {
            pReq3->guestInfo.interfaceVersion = VMMDEV_VERSION;
            pReq3->guestInfo.osType = enmOSType;

            rc = VbglGRPerform(&pReq3->header);
            VbglGRFree(&pReq3->header);
        }
    }

    return rc;
}

