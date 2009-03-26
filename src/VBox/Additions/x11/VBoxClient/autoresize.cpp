/** @file
 *
 * Guest client: display auto-resize.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

/** @todo this should probably be replaced by something IPRT */
/* For system() and WEXITSTATUS() */
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include <VBox/log.h>
#include <VBox/VBoxGuest.h>
#include <iprt/assert.h>
#include <iprt/thread.h>

#include "VBoxClient.h"

static int initAutoResize()
{
    int rc = VINF_SUCCESS, rcSystem, rcErrno;

    LogFlowFunc(("\n"));
    rcSystem = system("VBoxRandR --test");
    if (-1 == rcSystem)
    {
        rcErrno = errno;
        rc = RTErrConvertFromErrno(rcErrno);
    }
    if (RT_SUCCESS(rc))
    {
        if (0 != WEXITSTATUS(rcSystem))
            rc = VERR_NOT_SUPPORTED;
    }
    if (RT_SUCCESS(rc))
        rc = VbglR3CtlFilterMask(VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST, 0);
    LogFlowFunc(("returning %Rrc\n", rc));
    return rc;
}

void cleanupAutoResize(void)
{
    LogFlowFunc(("\n"));
    VbglR3CtlFilterMask(0, VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST);
    LogFlowFunc(("returning\n"));
}

/**
 * Display change request monitor thread function.
 * Before entering the loop, we re-read the last request
 * received, and if the first one received inside the
 * loop is identical we ignore it, because it is probably
 * stale.
 */
int runAutoResize()
{
    LogFlowFunc(("\n"));
    uint32_t cx0 = 0, cy0 = 0, cBits0 = 0, iDisplay0 = 0;
    int rc = VbglR3GetLastDisplayChangeRequest(&cx0, &cy0, &cBits0, &iDisplay0);
    while (true)
    {
        uint32_t cx = 0, cy = 0, cBits = 0, iDisplay = 0;
        rc = VbglR3DisplayChangeWaitEvent(&cx, &cy, &cBits, &iDisplay);
        /* Ignore the request if it is stale */
        if ((cx != cx0) || (cy != cy0))
        {
	        /* If we are not stopping, sleep for a bit to avoid using up too
	            much CPU while retrying. */
	        if (RT_FAILURE(rc))
	            RTThreadYield();
	        else
	            system("VBoxRandR");
        }
        /* We do not want to ignore any further requests. */
        cx0 = 0;
        cy0 = 0;
    }
    LogFlowFunc(("returning VINF_SUCCESS\n"));
    return VINF_SUCCESS;
}

class AutoResizeService : public VBoxClient::Service
{
public:
    virtual const char *getPidFilePath()
    {
        return ".vboxclient-autoresize.pid";
    }
    virtual int run()
    {
        int rc = initAutoResize();
        if (RT_SUCCESS(rc))
            rc = runAutoResize();
        return rc;
    }
    virtual void cleanup()
    {
        cleanupAutoResize();
    }
};

VBoxClient::Service *VBoxClient::GetAutoResizeService()
{
    return new AutoResizeService;
}
