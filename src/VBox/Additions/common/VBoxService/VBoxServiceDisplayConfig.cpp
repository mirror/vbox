/** @file
 * VBoxService - Guest displays handling.
 */

/*
 * Copyright (C) 2006-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <iprt/errcore.h>

#include <VBox/VBoxGuestLib.h>
#include "VBoxServiceInternal.h"
#include "VBoxServiceUtils.h"


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/

/**
 * @interface_method_impl{VBOXSERVICE,pfnInit}
 */
static DECLCALLBACK(int) vgsvcDisplayConfigInit(void)
{
    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{VBOXSERVICE,pfnWorker}
 */
DECLCALLBACK(int) vgsvcDisplayConfigWorker(bool volatile *pfShutdown)
{
    int rc = VINF_SUCCESS;

    /*
     * Tell the control thread that it can continue spawning services.
     */
    RTThreadUserSignal(RTThreadSelf());
    /*
     * The Work Loop.
     */

    rc = VbglR3CtlFilterMask(VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST, 0);
    VGSvcVerbose(2, "VbglR3CtlFilterMask set rc=%Rrc\n", rc);

    for (;;)
    {
        uint32_t fEvents = 0;

        rc = VbglR3AcquireGuestCaps(VMMDEV_GUEST_SUPPORTS_GRAPHICS, 0, false);
        VGSvcVerbose(2, "VbglR3AcquireGuestCaps acquire VMMDEV_GUEST_SUPPORTS_GRAPHICS rc=%Rrc\n", rc);

        rc = VbglR3WaitEvent(VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST, 1000 /*ms*/, &fEvents);
        VGSvcVerbose(2, "VbglR3WaitEvent rc=%Rrc\n", rc);

        if (RT_SUCCESS(rc))
        {
            VMMDevDisplayDef aDisplays[64];
            uint32_t cDisplays = RT_ELEMENTS(aDisplays);

            rc = VbglR3GetDisplayChangeRequestMulti(cDisplays, &cDisplays, &aDisplays[0], true);
            VGSvcVerbose(2, "VbglR3GetDisplayChangeRequestMulti rc=%Rrc cDisplays=%d\n", rc, cDisplays);
            if (cDisplays > 0)
            {
                VGSvcVerbose(2, "Display[0] (%dx%d)\n", aDisplays[0].cx, aDisplays[0].cy);
            }
        }

        rc = VbglR3AcquireGuestCaps(0, VMMDEV_GUEST_SUPPORTS_GRAPHICS, false);
        VGSvcVerbose(2, "VbglR3AcquireGuestCaps release VMMDEV_GUEST_SUPPORTS_GRAPHICS rc=%Rrc\n", rc);

        RTThreadSleep(1000);

        if (*pfShutdown)
            break;
    }

    rc = VbglR3CtlFilterMask(0, VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST);
    VGSvcVerbose(2, "VbglR3CtlFilterMask cleared rc=%Rrc\n", rc);

    return rc;
}

/**
 * @interface_method_impl{VBOXSERVICE,pfnStop}
 */
static DECLCALLBACK(void) vgsvcDisplayConfigStop(void)
{
}

/**
 * @interface_method_impl{VBOXSERVICE,pfnTerm}
 */
static DECLCALLBACK(void) vgsvcDisplayConfigTerm(void)
{
}

/**
 * The 'displayconfig' service description.
 */
VBOXSERVICE g_DisplayConfig =
{
    /* pszName. */
    "displayconfig",
    /* pszDescription. */
    "Display configuration",
    /* pszUsage. */
    NULL,
    /* pszOptions. */
    NULL,
    /* methods */
    VGSvcDefaultPreInit,
    VGSvcDefaultOption,
    vgsvcDisplayConfigInit,
    vgsvcDisplayConfigWorker,
    vgsvcDisplayConfigStop,
    vgsvcDisplayConfigTerm
};
