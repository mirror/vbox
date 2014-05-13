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
#include <X11/Xatom.h>
#include <X11/cursorfont.h>

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <VBox/log.h>
#include <VBox/VMMDev.h>
#include <VBox/VBoxGuestLib.h>

#include "VBoxClient.h"

/** Exit with a fatal error.
 * @todo Make this application global. */
#define FatalError(format) \
do { \
    char *pszMessage = RTStrAPrintf2 format; \
    LogRel(format); \
    doFatalError(pszMessage); \
} while(0)

static void doFatalError(char *pszMessage)
{
    char *pszCommand;
    if (pszMessage)
    {
        pszCommand = RTStrAPrintf2("notify-send \"VBoxClient: %s\"",
                                   pszMessage);
        if (pszCommand)
            system(pszCommand);
    }
    exit(1);
}

/** Tell the VBoxGuest driver we no longer want any events and tell the host
 * we no longer support any capabilities. */
static int disableEventsAndCaps()
{
    int rc = VbglR3SetGuestCaps(0, VMMDEV_GUEST_SUPPORTS_GRAPHICS);
    if (RT_FAILURE(rc))
        FatalError(("Failed to unset graphics capability, rc=%Rrc.\n", rc));
    rc = VbglR3SetMouseStatus(VMMDEV_MOUSE_GUEST_NEEDS_HOST_CURSOR);
    if (RT_FAILURE(rc))
        FatalError(("Failed to unset mouse status, rc=%Rrc.\n", rc));
    rc = VbglR3CtlFilterMask(0,  VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED
                                | VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST);
    if (RT_FAILURE(rc))
        FatalError(("Failed to unset filter mask, rc=%Rrc.\n", rc));
    return VINF_SUCCESS;
}

/** Tell the VBoxGuest driver which events we want and tell the host which
 * capabilities we support. */
static int enableEventsAndCaps(Display *pDisplay)
{
    int rc = VbglR3CtlFilterMask(  VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED
                                 | VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST, 0);
    if (RT_FAILURE(rc))
        FatalError(("Failed to set filter mask, rc=%Rrc.\n", rc));
    rc = VbglR3SetGuestCaps(VMMDEV_GUEST_SUPPORTS_GRAPHICS, 0);
    if (RT_FAILURE(rc))
        FatalError(("Failed to set graphics capability, rc=%Rrc.\n", rc));
    rc = VbglR3SetMouseStatus(0);
    if (RT_FAILURE(rc))
        FatalError(("Failed to set mouse status, rc=%Rrc.\n", rc));
    return VINF_SUCCESS;
}

/**
 * Display change request monitor thread function.
 * Before entering the loop, we re-read the last request
 * received, and if the first one received inside the
 * loop is identical we ignore it, because it is probably
 * stale.
 */
static void runDisplay(Display *pDisplay)
{
    int status, rc;
    char szCommand[256];
    Cursor hClockCursor = XCreateFontCursor(pDisplay, XC_watch);
    Cursor hArrowCursor = XCreateFontCursor(pDisplay, XC_left_ptr);
    LogRelFlowFunc(("\n"));
    bool fExtDispReqSupport = true, fHaveRandR12 = false;
    const char *pcszXrandr = "xrandr";
    if (RTFileExists("/usr/X11/bin/xrandr"))
        pcszXrandr = "/usr/X11/bin/xrandr";
    status = system(pcszXrandr);
    if (WEXITSTATUS(status) != 0)  /* Utility or extension not available. */
        FatalError(("Failed to execute the xrandr utility.\n"));
    RTStrPrintf(szCommand, sizeof(szCommand), "%s --q12", pcszXrandr);
    status = system(szCommand);
    if (WEXITSTATUS(status) == 0)
        fHaveRandR12 = true;
    while (true)
    {
        uint32_t fEvents;
        /** The size of our array of size hints. */
        unsigned cSizeHints = 0;
        /** Array of size hints.  Large enough to hold the highest display
         * number we have had a hint for so far, reallocated when a higher one
         * comes.  Zero means no hint for that display. */
        long *paSizeHints = NULL;
        do
            rc = VbglR3WaitEvent(  VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST
                                 | VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED,
                                 RT_INDEFINITE_WAIT, &fEvents);
        while(rc == VERR_INTERRUPTED);
        if (RT_FAILURE(rc))  /* VERR_NO_MEMORY? */
            FatalError(("event wait failed, rc=%Rrc\n", rc));
        if (fEvents & VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED)
        {
            /* Jiggle the mouse pointer to trigger a switch to a software
             * cursor if necessary. */
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
        /* And if it is a size hint, set the new size. */
        if (fEvents & VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST)
        {
            uint32_t cx = 0, cy = 0, cBits = 0, iDisplay = 0, cxOrg = 0,
                     cyOrg = 0;
            bool fEnabled = false;
            if (fExtDispReqSupport)
            {
                int rc2 = VbglR3GetDisplayChangeRequestEx(&cx, &cy, &cBits,
                                                          &iDisplay, &cxOrg,
                                                          &cyOrg, &fEnabled,
                                                          true);
                /* Extended display version not supported on host */
                if (RT_FAILURE(rc2))
                {
                    if (rc2 != VERR_NOT_IMPLEMENTED)
                        FatalError(("Failed to get display change request, rc=%Rrc\n",
                                    rc));
                    LogRel(("Extended display change request not supported.\n"));
                    fExtDispReqSupport = false;
                }
                else
                    LogRelFlowFunc(("Got Extended Param from Host cx=%d, cy=%d, bpp=%d, iDisp=%d, OrgX=%d, OrgY=%d Enb=%d\n",
                                    cx, cy, cBits, iDisplay, cxOrg, cyOrg,
                                    fEnabled));
            }
            if (!fExtDispReqSupport)
                rc = VbglR3GetDisplayChangeRequest(&cx, &cy, &cBits, &iDisplay,
                                                   true);
            if (RT_FAILURE(rc))
                FatalError(("Failed to retrieve size hint, rc=%Rrc\n", rc));
            if (iDisplay > INT32_MAX)
                FatalError(("Received a hint for too high display number %u\n",
                            (unsigned) iDisplay));
            if (iDisplay >= cSizeHints)
            {
                uint32_t i;

                paSizeHints = (long *)RTMemRealloc(paSizeHints,
                                                     (iDisplay + 1)
                                                   * sizeof(*paSizeHints));
                if (!paSizeHints)
                    FatalError(("Failed to re-allocate size hint memory.\n"));
                for (i = cSizeHints; i < iDisplay + 1; ++i)
                    paSizeHints[i] = 0;
                cSizeHints = iDisplay + 1;
            }
            if ((!fExtDispReqSupport || fEnabled) && cx != 0 && cy != 0)
            {
                paSizeHints[iDisplay] = (cx & 0xffff) << 16 | (cy & 0xffff);
                XChangeProperty(pDisplay, DefaultRootWindow(pDisplay),
                                XInternAtom(pDisplay, "VBOX_SIZE_HINTS", 0),
                                XA_INTEGER, 32, PropModeReplace,
                                (unsigned char *)paSizeHints, cSizeHints);
                XFlush(pDisplay);
            }
            if (!fHaveRandR12)
            {
                RTStrPrintf(szCommand, sizeof(szCommand),
                            "%s -s %ux%u", pcszXrandr, cx, cy);
                status = system(szCommand);
                if (WEXITSTATUS(status) != 0)
                    FatalError(("Failed to execute \\\"%s\\\".\n", szCommand));
            }
            else
            {
                if (fExtDispReqSupport && fEnabled)
                {
                    /* Extended Display support possible . Secondary monitor
                     * position supported */
                    RTStrPrintf(szCommand, sizeof(szCommand),
                                "%s --output VGA-%u --auto --pos %dx%d",
                                pcszXrandr, iDisplay, cxOrg, cyOrg);
                    status = system(szCommand);
                    if (WEXITSTATUS(status) != 0)
                        FatalError(("Failed to execute \\\"%s\\\".\n", szCommand));
                }
                if ((!fExtDispReqSupport || fEnabled) && cx != 0 && cy != 0)
                {
                    RTStrPrintf(szCommand, sizeof(szCommand),
                                "%s --output VGA-%u --preferred",
                                pcszXrandr, iDisplay);
                    status = system(szCommand);
                    if (WEXITSTATUS(status) != 0)
                        FatalError(("Failed to execute \\\"%s\\\".\n", szCommand));
                }
                if (fExtDispReqSupport && !fEnabled)
                {
                    /* disable the virtual monitor */
                    RTStrPrintf(szCommand, sizeof(szCommand),
                                "%s --output VGA-%u --off",
                                 pcszXrandr, iDisplay);
                    status = system(szCommand);
                    if (WEXITSTATUS(status) != 0)
                        FatalError(("Failed to execute \\\"%s\\\".\n", szCommand));
                }
            }
        }
    }
}

class DisplayService : public VBoxClient::Service
{
    Display *mDisplay;
    bool mfInit;
public:
    virtual const char *getPidFilePath()
    {
        return ".vboxclient-display.pid";
    }
    virtual int init()
    {
        int rc;
        
        if (mfInit)
            return VERR_WRONG_ORDER;
        mDisplay = XOpenDisplay(NULL);
        if (!mDisplay)
            return VERR_NOT_FOUND;
        rc = enableEventsAndCaps(mDisplay);
        if (RT_SUCCESS(rc))
            mfInit = true;
        return rc;
    }
    virtual int run(bool fDaemonised /* = false */)
    {
        if (!mfInit)
            return VERR_WRONG_ORDER;
        runDisplay(mDisplay);
        return VERR_INTERNAL_ERROR;  /* "Should never reach here." */
    }
    virtual int pause()
    {
        if (!mfInit)
            return VERR_WRONG_ORDER;
        return disableEventsAndCaps();
    }
    virtual int resume()
    {
        if (!mfInit)
            return VERR_WRONG_ORDER;
        return enableEventsAndCaps(mDisplay);
    }
    virtual void cleanup()
    {
        disableEventsAndCaps();
    }
    DisplayService() { mfInit = false; }
};

VBoxClient::Service *VBoxClient::GetDisplayService()
{
    return new DisplayService;
}
