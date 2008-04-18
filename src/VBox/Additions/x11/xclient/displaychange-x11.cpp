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

#include <VBox/log.h>
#include <VBox/VBoxGuest.h>
#include <iprt/assert.h>

/** @todo this should probably be replaced by something IPRT */
/* For system() and WEXITSTATUS() */
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include "displaychange.h"

int VBoxGuestDisplayChangeThreadX11::init(void)
{
    int rc = VINF_SUCCESS, rcSystem, rcErrno;

    LogFlowThisFunc(("\n"));
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
    if (RT_SUCCESS(rc))
        mInit = true;
    LogFlowThisFunc(("returning %Rrc\n", rc));
    return rc;
}

void VBoxGuestDisplayChangeThreadX11::uninit(void)
{
    LogFlowThisFunc(("\n"));
    VbglR3CtlFilterMask(0, VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST);
    mInit = false;
    LogFlowThisFunc(("returning\n"));
}

/**
 * Display change request monitor thread function
 */
int VBoxGuestDisplayChangeThreadX11::threadFunction(VBoxGuestThread *pThread)
{
    mThread = pThread;
    LogFlowThisFunc(("\n"));
    while (!mThread->isStopping())
    {
        uint32_t cx, cy, cBits, iDisplay;
        int rc = VbglR3DisplayChangeWaitEvent(&cx, &cy, &cBits, &iDisplay);
        /* If we are not stopping, sleep for a bit to avoid using up too
            much CPU while retrying. */
        if (RT_FAILURE(rc) && !mThread->isStopping())
            mThread->yield();
        else
            system("VBoxRandR");
    }
    LogFlowThisFunc(("returning VINF_SUCCESS\n"));
    return VINF_SUCCESS;
}

/**
 * Send a signal to the thread function that it should exit
 */
void VBoxGuestDisplayChangeThreadX11::stop(void)
{
    /**
     * @todo is this reasonable?  If the thread is in the event loop then the cancelEvent()
     *       will cause it to exit.  If it enters or exits the event loop it will also
     *       notice that we wish it to exit.  And if it is somewhere in-between, the
     *       yield() should give it time to get to one of places mentioned above.
     */
    LogFlowThisFunc(("\n"));
    for (int i = 0; (i < 5) && mThread->isRunning(); ++i)
    {
        VbglR3InterruptEventWaits();;
        mThread->yield();
    }
    LogFlowThisFunc(("returning\n"));
}

int VBoxGuestDisplayChangeMonitor::init(void)
{
    int rc = VINF_SUCCESS;

    LogFlowThisFunc(("\n"));
    if (mInit)
        return VINF_SUCCESS;
    rc = mThreadFunction.init();
    if (RT_FAILURE(rc))
        Log(("VBoxClient: failed to initialise the display change thread, rc=%Rrc (VBoxGuestDisplayChangeMonitor::init)\n", rc));
    if (RT_SUCCESS(rc))
    {
        rc = mThread.start();
        if (RT_FAILURE(rc))
            Log(("VBoxClient: failed to start the display change thread, rc=%Rrc (VBoxGuestDisplayChangeMonitor::init)\n", rc));
    }
    if (RT_SUCCESS(rc))
        mInit = true;
    LogFlowThisFunc(("returning %Rrc\n, rc"));
    return rc;
}

void VBoxGuestDisplayChangeMonitor::uninit(unsigned cMillies /* = RT_INDEFINITE_WAIT */)
{
    LogFlowThisFunc(("\n"));
    if (mInit)
    {
        if (mThread.stop(cMillies, 0))
            mThreadFunction.uninit();
    }
    LogFlowThisFunc(("returning\n"));
}
