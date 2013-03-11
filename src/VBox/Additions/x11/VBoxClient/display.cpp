/* $Id$ */
/** @file
 * X11 guest client - display management.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/** @todo this should probably be replaced by something IPRT */
/* For system() and WEXITSTATUS() */
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xrandr.h>

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <VBox/log.h>
#include <VBox/VMMDev.h>
#include <VBox/VBoxGuestLib.h>

#include "VBoxClient.h"

static int initDisplay(Display *pDisplay)
{
    int rc = VINF_SUCCESS;
    uint32_t fMouseFeatures = 0;

    LogRelFlowFunc(("testing dynamic resizing\n"));
    int iDummy;
    if (!XRRQueryExtension(pDisplay, &iDummy, &iDummy))
        rc = VERR_NOT_SUPPORTED;
    if (RT_SUCCESS(rc))
        rc = VbglR3CtlFilterMask(VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST, 0);
    else
        VbglR3CtlFilterMask(0, VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST);
    /* Log and ignore the return value, as there is not much we can do with
     * it. */
    LogRelFlowFunc(("dynamic resizing: result %Rrc\n", rc));
    /* Enable support for switching between hardware and software cursors */
    LogRelFlowFunc(("enabling relative mouse re-capturing support\n"));
    rc = VbglR3GetMouseStatus(&fMouseFeatures, NULL, NULL);
    if (RT_SUCCESS(rc))
    {
        rc = VbglR3CtlFilterMask(VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED,
                                 0);
        if (RT_SUCCESS(rc))
            rc = VbglR3SetMouseStatus
                               (  fMouseFeatures
                                & ~VMMDEV_MOUSE_GUEST_NEEDS_HOST_CURSOR);
    }
    if (RT_FAILURE(rc))
    {
        VbglR3CtlFilterMask(0, VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED);
        VbglR3SetMouseStatus(  fMouseFeatures
                             | VMMDEV_MOUSE_GUEST_NEEDS_HOST_CURSOR);
    }
    LogRelFlowFunc(("mouse re-capturing support: result %Rrc\n", rc));
    return VINF_SUCCESS;
}

void cleanupDisplay(void)
{
    uint32_t fMouseFeatures = 0;
    LogRelFlowFunc(("\n"));
    VbglR3CtlFilterMask(0,   VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST
                           | VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED);
    int rc = VbglR3GetMouseStatus(&fMouseFeatures, NULL, NULL);
    if (RT_SUCCESS(rc))
        VbglR3SetMouseStatus(  fMouseFeatures
                             | VMMDEV_MOUSE_GUEST_NEEDS_HOST_CURSOR);
    LogRelFlowFunc(("returning\n"));
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
 * This method first resets the current resolution using RandR to wake up
 * the graphics driver, then sets the resolution requested if it is among
 * those offered by the driver.
 */
static void setSize(Display *pDisplay, uint32_t cx, uint32_t cy)
{
    XRRScreenConfiguration *pConfig;
    XRRScreenSize *pSizes;
    int cSizes;
    pConfig = XRRGetScreenInfo(pDisplay, DefaultRootWindow(pDisplay));
    /* Reset the current mode */
    LogRelFlowFunc(("Setting size %ux%u\n", cx, cy));
    if (pConfig)
    {
        pSizes = XRRConfigSizes(pConfig, &cSizes);
        unsigned uDist = UINT32_MAX;
        int iMode = -1;
        for (int i = 0; i < cSizes; ++i)
        {
#define VBCL_SQUARE(x) (x) * (x)
            unsigned uThisDist =   VBCL_SQUARE(pSizes[i].width - cx)
                                 + VBCL_SQUARE(pSizes[i].height - cy);
            LogRelFlowFunc(("Found size %dx%d, distance %u\n", pSizes[i].width,
                         pSizes[i].height, uThisDist));
#undef VBCL_SQUARE
            if (uThisDist < uDist)
            {
                uDist = uThisDist;
                iMode = i;
            }
        }
        if (iMode >= 0)
        {
            Time config_timestamp = 0;
            XRRConfigTimes(pConfig, &config_timestamp);
            LogRelFlowFunc(("Setting new size %d\n", iMode));
            XRRSetScreenConfig(pDisplay, pConfig,
                               DefaultRootWindow(pDisplay), iMode,
                               RR_Rotate_0, config_timestamp);
        }
        XRRFreeScreenConfigInfo(pConfig);
    }
}

/**
 * Display change request monitor thread function.
 * Before entering the loop, we re-read the last request
 * received, and if the first one received inside the
 * loop is identical we ignore it, because it is probably
 * stale.
 */
static int runDisplay(Display *pDisplay)
{
    LogRelFlowFunc(("\n"));
    Cursor hClockCursor = XCreateFontCursor(pDisplay, XC_watch);
    Cursor hArrowCursor = XCreateFontCursor(pDisplay, XC_left_ptr);
    int RRMaj, RRMin;
    bool fExtDispReqSupport = true;
    if (!XRRQueryVersion(pDisplay, &RRMaj, &RRMin))
        RRMin = 0;
    const char *pcszXrandr = "xrandr";
    if (RTFileExists("/usr/X11/bin/xrandr"))
        pcszXrandr = "/usr/X11/bin/xrandr";
    int rc = RTThreadCreate(NULL, x11ConnectionMonitor, NULL, 0,
                   RTTHREADTYPE_INFREQUENT_POLLER, 0, "X11 monitor");
    if (RT_FAILURE(rc))
        return rc;
    while (true)
    {
        uint32_t fEvents = 0, cx = 0, cy = 0, cBits = 0, iDisplay = 0, cxOrg = 0, cyOrg = 0;
        bool fEnabled = false;
        rc = VbglR3WaitEvent(  VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST
                             | VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED,
                             RT_INDEFINITE_WAIT, &fEvents);
        if (RT_FAILURE(rc) && rc != VERR_INTERRUPTED)  /* VERR_NO_MEMORY? */
            return rc;
        /* Jiggle the mouse pointer to wake up the driver. */
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
        /* And if it is a size hint, set the new size now that the video
         * driver has had a chance to update its list. */
        if (RT_SUCCESS(rc) && (fEvents & VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST))
        {
            int rc2 = VbglR3GetDisplayChangeRequestEx(&cx, &cy, &cBits,
                                                      &iDisplay, &cxOrg, &cyOrg, &fEnabled, true);
            /* Extended display version not supported on host */
            if (RT_FAILURE(rc2))
            {
                LogRel(("GetDisplayChangeReq Extended Version not supported.  "
                        "Trying for Normal Mode with cx=%d & cy=%d\n", cx, cy));
                fExtDispReqSupport = false;
                rc2 = VbglR3GetDisplayChangeRequest(&cx, &cy, &cBits, &iDisplay, true);
            }
            else
                LogRelFlowFunc(("Got Extended Param from Host cx=%d, cy=%d, bpp=%d, iDisp=%d, "
                                "OrgX=%d, OrgY=%d Enb=%d\n", cx, cy, cBits, iDisplay,
                                cxOrg, cyOrg, fEnabled));
            /* If we are not stopping, sleep for a bit to avoid using up
                too much CPU while retrying. */
            if (RT_FAILURE(rc2))
                RTThreadYield();
            else
                if (RRMin < 2)
                    setSize(pDisplay, cx, cy);
                else
                {
                    char szCommand[256];
                    if (fExtDispReqSupport)
                    {
                        if (fEnabled)
                        {
                            if (cx != 0 && cy != 0)
                            {
                                RTStrPrintf(szCommand, sizeof(szCommand),
                                            "%s --output VBOX%u --set VBOX_MODE %dx%d",
                                            pcszXrandr, iDisplay, cx, cy);
                                system(szCommand);
                            }
                            /* Extended Display support possible . Secondary monitor position supported */
                            if (cxOrg != 0 || cyOrg != 0)
                            {
                                RTStrPrintf(szCommand, sizeof(szCommand),
                                            "%s --output VBOX%u --auto --pos %dx%d",
                                            pcszXrandr, iDisplay, cxOrg, cyOrg);
                                system(szCommand);
                            }
                            RTStrPrintf(szCommand, sizeof(szCommand),
                                        "%s --output VBOX%u --preferred",
                                        pcszXrandr, iDisplay);
                            system(szCommand);
                        }
                        else /* disable the virtual monitor */
                        {
                            RTStrPrintf(szCommand, sizeof(szCommand),
                                        "%s --output VBOX%u --off",
                                         pcszXrandr, iDisplay);
                            system(szCommand);
                        }
                    }
                    else /* Extended display support not possible */
                    {
                        if (cx != 0 && cy != 0)
                        {
                            RTStrPrintf(szCommand, sizeof(szCommand),
                                        "%s --output VBOX%u --set VBOX_MODE %dx%d",
                                        pcszXrandr, iDisplay, cx, cy);
                            system(szCommand);
                            RTStrPrintf(szCommand, sizeof(szCommand),
                                        "%s --output VBOX%u --preferred",
                                        pcszXrandr, iDisplay);
                            system(szCommand);
                        }
                    }

                }
        }
    }
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
        Display *pDisplay = XOpenDisplay(NULL);
        if (!pDisplay)
            return VERR_NOT_FOUND;
        int rc = initDisplay(pDisplay);
        if (RT_SUCCESS(rc))
            rc = runDisplay(pDisplay);
        XCloseDisplay(pDisplay);
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
