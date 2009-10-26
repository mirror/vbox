/* $Id$ */
/** @file
 * X11 guest client - display management.
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

#include <X11/Xlib.h>
#include <X11/cursorfont.h>

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/thread.h>
#include <VBox/log.h>
#include <VBox/VMMDev.h>
#include <VBox/VBoxGuestLib.h>

#include "VBoxClient.h"

static int initDisplay()
{
    int rc = VINF_SUCCESS;
    int rcSystem, rcErrno;
    uint32_t fMouseFeatures = 0;

    LogFlowFunc(("enabling dynamic resizing\n"));
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
    /* Log and ignore the return value, as there is not much we can do with
     * it. */
    LogFlowFunc(("dynamic resizing: result %Rrc\n", rc));
    /* Enable support for switching between hardware and software cursors */
    LogFlowFunc(("enabling relative mouse re-capturing support\n"));
    rc = VbglR3GetMouseStatus(&fMouseFeatures, NULL, NULL);
    if (RT_SUCCESS(rc))
    {
        if (fMouseFeatures & VMMDEV_MOUSE_HOST_RECHECKS_NEEDS_HOST_CURSOR)
        {
            rc = VbglR3CtlFilterMask(VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED,
                                     0);
            if (RT_SUCCESS(rc))
                rc = VbglR3SetMouseStatus
                                   (  fMouseFeatures
                                    & ~VMMDEV_MOUSE_GUEST_NEEDS_HOST_CURSOR);
        }
        else
            rc = VERR_NOT_SUPPORTED;
    }
    LogFlowFunc(("mouse re-capturing support: result %Rrc\n", rc));
    return VINF_SUCCESS;
}

void cleanupDisplay(void)
{
    uint32_t fMouseFeatures = 0;
    LogFlowFunc(("\n"));
    VbglR3CtlFilterMask(0,   VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST
                           | VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED);
    int rc = VbglR3GetMouseStatus(&fMouseFeatures, NULL, NULL);
    if (RT_SUCCESS(rc))
        VbglR3SetMouseStatus(  fMouseFeatures
                             | VMMDEV_MOUSE_GUEST_NEEDS_HOST_CURSOR);
    LogFlowFunc(("returning\n"));
}

/** This thread just runs a dummy X11 event loop to be sure that we get
 * terminated should the X server exit. */
static int x11ConnectionMonitor(RTTHREAD, void *)
{
    XEvent ev;
    Display *pDisplay = XOpenDisplay(NULL);
    while (true)
        XNextEvent(pDisplay, &ev);
    return 0;
}

/**
 * Display change request monitor thread function.
 * Before entering the loop, we re-read the last request
 * received, and if the first one received inside the
 * loop is identical we ignore it, because it is probably
 * stale.
 */
int runDisplay()
{
    LogFlowFunc(("\n"));
    uint32_t cx0 = 0, cy0 = 0, cBits0 = 0, iDisplay0 = 0;
    Display *pDisplay = XOpenDisplay(NULL);
    if (pDisplay == NULL)
        return VERR_NOT_FOUND;
    Cursor hClockCursor = XCreateFontCursor(pDisplay, XC_watch);
    Cursor hArrowCursor = XCreateFontCursor(pDisplay, XC_left_ptr);
    int rc = RTThreadCreate(NULL, x11ConnectionMonitor, NULL, 0,
                   RTTHREADTYPE_INFREQUENT_POLLER, 0, "X11 monitor");
    if (RT_FAILURE(rc))
        return rc;
    VbglR3GetDisplayChangeRequest(&cx0, &cy0, &cBits0, &iDisplay0, false);
    while (true)
    {
        uint32_t fEvents = 0, cx = 0, cy = 0, cBits = 0, iDisplay = 0;
        rc = VbglR3WaitEvent(  VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST
                             | VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED,
                             RT_INDEFINITE_WAIT, &fEvents);
        if (RT_SUCCESS(rc) && (fEvents & VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST))
        {
            int rc2 = VbglR3GetDisplayChangeRequest(&cx, &cy, &cBits,
                                                    &iDisplay, true);
            /* Ignore the request if it is stale */
            if ((cx != cx0) || (cy != cy0) || RT_FAILURE(rc2))
            {
	            /* If we are not stopping, sleep for a bit to avoid using up
	                too much CPU while retrying. */
	            if (RT_FAILURE(rc2))
	                RTThreadYield();
	            else
	            {
	                system("VBoxRandR");
                    cx0 = cx;
                    cy0 = cy;
	            }
            }
        }
        if (   RT_SUCCESS(rc)
            && (fEvents & VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED))
        {
            XGrabPointer(pDisplay,
                         DefaultRootWindow(pDisplay), true, 0, GrabModeAsync,
                         GrabModeAsync, None, hClockCursor, CurrentTime);
            XFlush(pDisplay);
            XGrabPointer(pDisplay,
                         DefaultRootWindow(pDisplay), true, 0, GrabModeAsync,
                         GrabModeAsync, None, hArrowCursor, CurrentTime);
            XFlush(pDisplay);
            XUngrabPointer(pDisplay, CurrentTime);
            XFlush(pDisplay);
        }
    }
    LogFlowFunc(("returning VINF_SUCCESS\n"));
    return VINF_SUCCESS;
}

class DisplayService : public VBoxClient::Service
{
public:
    virtual const char *getPidFilePath()
    {
        return ".vboxclient-display.pid";
    }
    virtual int run(bool fDaemonised /* = false */)
    {
        int rc = initDisplay();
        if (RT_SUCCESS(rc))
            rc = runDisplay();
        return rc;
    }
    virtual void cleanup()
    {
        cleanupDisplay();
    }
};

VBoxClient::Service *VBoxClient::GetDisplayService()
{
    return new DisplayService;
}
