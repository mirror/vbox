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

/** State information we need for interacting with the X server. */
struct x11State
{
    /** The connection to the server. */
    Display *pDisplay;
    /** Can we use version 1.2 or later of the RandR protocol here? */
    bool fHaveRandR12;
    /** The command argument to use for the xrandr binary.  Currently only
     * used to support the non-standard location on some Solaris systems -
     * would it make sense to use absolute paths on all systems? */
    const char *pcszXrandr;
    /** The size of our array of size hints. */
    unsigned cSizeHints;
    /** Array of size hints.  Large enough to hold the highest display
     * number we have had a hint for so far, reallocated when a higher one
     * comes.  Zero means no hint for that display. */
    long *paSizeHints;
};

/** Tell the VBoxGuest driver we no longer want any events and tell the host
 * we no longer support any capabilities. */
static int disableEventsAndCaps()
{
    int rc = VbglR3SetGuestCaps(0, VMMDEV_GUEST_SUPPORTS_GRAPHICS);
    if (RT_FAILURE(rc))
        VBClFatalError(("Failed to unset graphics capability, rc=%Rrc.\n", rc));
    rc = VbglR3SetMouseStatus(VMMDEV_MOUSE_GUEST_NEEDS_HOST_CURSOR);
    if (RT_FAILURE(rc))
        VBClFatalError(("Failed to unset mouse status, rc=%Rrc.\n", rc));
    rc = VbglR3CtlFilterMask(0,  VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED
                                | VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST);
    if (RT_FAILURE(rc))
        VBClFatalError(("Failed to unset filter mask, rc=%Rrc.\n", rc));
    return VINF_SUCCESS;
}

/** Tell the VBoxGuest driver which events we want and tell the host which
 * capabilities we support. */
static int enableEventsAndCaps()
{
    int rc = VbglR3CtlFilterMask(  VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED
                                 | VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST, 0);
    if (RT_FAILURE(rc))
        VBClFatalError(("Failed to set filter mask, rc=%Rrc.\n", rc));
    rc = VbglR3SetGuestCaps(VMMDEV_GUEST_SUPPORTS_GRAPHICS, 0);
    if (RT_FAILURE(rc))
        VBClFatalError(("Failed to set graphics capability, rc=%Rrc.\n", rc));
    rc = VbglR3SetMouseStatus(0);
    if (RT_FAILURE(rc))
        VBClFatalError(("Failed to set mouse status, rc=%Rrc.\n", rc));
    return VINF_SUCCESS;
}

static int initX11(struct x11State *pState)
{
    char szCommand[256];
    int status;

    pState->pDisplay = XOpenDisplay(NULL);
    if (!pState->pDisplay)
        return VERR_NOT_FOUND;
    pState->fHaveRandR12 = false;
    pState->pcszXrandr = "xrandr";
    if (RTFileExists("/usr/X11/bin/xrandr"))
        pState->pcszXrandr = "/usr/X11/bin/xrandr";
    status = system(pState->pcszXrandr);
    if (WEXITSTATUS(status) != 0)  /* Utility or extension not available. */
        VBClFatalError(("Failed to execute the xrandr utility.\n"));
    RTStrPrintf(szCommand, sizeof(szCommand), "%s --q12", pState->pcszXrandr);
    status = system(szCommand);
    if (WEXITSTATUS(status) == 0)
        pState->fHaveRandR12 = true;
    pState->cSizeHints = 0;
    pState->paSizeHints = NULL;
    return VINF_SUCCESS;
}

static void setModeX11(struct x11State *pState, unsigned cx, unsigned cy,
                       unsigned cBPP, unsigned iDisplay, unsigned x,
                       unsigned y, bool fEnabled, bool fSetPosition)
{
    char szCommand[256];
    int status;
    uint32_t i;

    if (iDisplay >= pState->cSizeHints)
    {
        pState->paSizeHints = (long *)RTMemRealloc(pState->paSizeHints,
                                                  (iDisplay + 1)
                                                * sizeof(*pState->paSizeHints));
        if (!pState->paSizeHints)
            VBClFatalError(("Failed to re-allocate size hint memory.\n"));
        for (i = pState->cSizeHints; i < iDisplay + 1; ++i)
            pState->paSizeHints[i] = 0;
        pState->cSizeHints = iDisplay + 1;
    }
    if ((!fSetPosition || fEnabled) && cx != 0 && cy != 0)
    {
        pState->paSizeHints[iDisplay] = (cx & 0xffff) << 16 | (cy & 0xffff);
        XChangeProperty(pState->pDisplay, DefaultRootWindow(pState->pDisplay),
                        XInternAtom(pState->pDisplay, "VBOX_SIZE_HINTS", 0),
                        XA_INTEGER, 32, PropModeReplace,
                        (unsigned char *)pState->paSizeHints,
                        pState->cSizeHints);
        XFlush(pState->pDisplay);
    }
    if (!pState->fHaveRandR12)
    {
        RTStrPrintf(szCommand, sizeof(szCommand),
                    "%s -s %ux%u", pState->pcszXrandr, cx, cy);
        status = system(szCommand);
        if (WEXITSTATUS(status) != 0)
            VBClFatalError(("Failed to execute \\\"%s\\\".\n", szCommand));
    }
    else
    {
        if (fSetPosition && fEnabled)
        {
            /* Extended Display support possible . Secondary monitor
             * position supported */
            RTStrPrintf(szCommand, sizeof(szCommand),
                        "%s --output VGA-%u --auto --pos %ux%u",
                        pState->pcszXrandr, iDisplay, x, y);
            status = system(szCommand);
            if (WEXITSTATUS(status) != 0)
                VBClFatalError(("Failed to execute \\\"%s\\\".\n", szCommand));
        }
        if ((!fSetPosition || fEnabled) && cx != 0 && cy != 0)
        {
            RTStrPrintf(szCommand, sizeof(szCommand),
                        "%s --output VGA-%u --preferred",
                        pState->pcszXrandr, iDisplay);
            status = system(szCommand);
            if (WEXITSTATUS(status) != 0)
                VBClFatalError(("Failed to execute \\\"%s\\\".\n", szCommand));
        }
        if (!fEnabled)
        {
            /* disable the virtual monitor */
            RTStrPrintf(szCommand, sizeof(szCommand),
                        "%s --output VGA-%u --off",
                         pState->pcszXrandr, iDisplay);
            status = system(szCommand);
            if (WEXITSTATUS(status) != 0)
                VBClFatalError(("Failed to execute \\\"%s\\\".\n", szCommand));
        }
    }
}

/**
 * Display change request monitor thread function.
 * Before entering the loop, we re-read the last request
 * received, and if the first one received inside the
 * loop is identical we ignore it, because it is probably
 * stale.
 */
static void runDisplay(struct x11State *pState)
{
    int status, rc;
    unsigned i, cScreens;
    char szCommand[256];
    Cursor hClockCursor = XCreateFontCursor(pState->pDisplay, XC_watch);
    Cursor hArrowCursor = XCreateFontCursor(pState->pDisplay, XC_left_ptr);

    LogRelFlowFunc(("\n"));
    rc = VbglR3VideoModeGetHighestSavedScreen(&cScreens);
    if (RT_FAILURE(rc))
        VBClFatalError(("Failed to get the number of saved screen modes, rc=%Rrc\n",
                    rc));
    for (i = 0; i < RT_MAX(cScreens + 1, 8); ++i)
    {
        unsigned cx = 0, cy = 0, cBPP = 0, x = 0, y = 0;
        bool fEnabled = true;

        rc = VbglR3RetrieveVideoMode(i, &cx, &cy, &cBPP, &x, &y,
                                     &fEnabled);
        if (RT_SUCCESS(rc) && i > cScreens) /* Sanity */
            VBClFatalError(("Internal error retrieving the number of saved screen modes.\n"));
        if (RT_SUCCESS(rc))
            setModeX11(pState, cx, cy, cBPP, i, x, y, fEnabled,
                       true);
    }
    while (true)
    {
        uint32_t fEvents;
        do
            rc = VbglR3WaitEvent(  VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST
                                 | VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED,
                                 RT_INDEFINITE_WAIT, &fEvents);
        while(rc == VERR_INTERRUPTED);
        if (RT_FAILURE(rc))  /* VERR_NO_MEMORY? */
            VBClFatalError(("event wait failed, rc=%Rrc\n", rc));
        if (fEvents & VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED)
        {
            /* Jiggle the mouse pointer to trigger a switch to a software
             * cursor if necessary. */
            XGrabPointer(pState->pDisplay,
                         DefaultRootWindow(pState->pDisplay), true, 0,
                         GrabModeAsync, GrabModeAsync, None, hClockCursor,
                         CurrentTime);
            XFlush(pState->pDisplay);
            XGrabPointer(pState->pDisplay,
                         DefaultRootWindow(pState->pDisplay), true, 0,
                         GrabModeAsync, GrabModeAsync, None, hArrowCursor,
                         CurrentTime);
            XFlush(pState->pDisplay);
            XUngrabPointer(pState->pDisplay, CurrentTime);
            XFlush(pState->pDisplay);
        }
        /* And if it is a size hint, set the new size. */
        if (fEvents & VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST)
        {
            uint32_t cx = 0, cy = 0, cBPP = 0, iDisplay = 0, x = 0, y = 0;
            bool fEnabled = true, fSetPosition = true;
            VMMDevSeamlessMode Mode;

            rc = VbglR3GetDisplayChangeRequestEx(&cx, &cy, &cBPP, &iDisplay,
                                                 &x, &y, &fEnabled,
                                                 true);
            /* Extended display version not supported on host */
            if (RT_FAILURE(rc))
            {
                if (rc != VERR_NOT_IMPLEMENTED)
                    VBClFatalError(("Failed to get display change request, rc=%Rrc\n",
                                rc));
                fSetPosition = false;
                rc = VbglR3GetDisplayChangeRequest(&cx, &cy, &cBPP,
                                                   &iDisplay, true);
                if (RT_SUCCESS(rc))
                    LogRelFlowFunc(("Got size hint from host cx=%d, cy=%d, bpp=%d, iDisplay=%d\n",
                                    cx, cy, cBPP, iDisplay));
            }
            else
                LogRelFlowFunc(("Got extended size hint from host cx=%d, cy=%d, bpp=%d, iDisplay=%d, x=%d, y=%d fEnabled=%d\n",
                                cx, cy, cBPP, iDisplay, x, y,
                                fEnabled));
            if (RT_FAILURE(rc))
                VBClFatalError(("Failed to retrieve size hint, rc=%Rrc\n", rc));
            if (iDisplay > INT32_MAX)
                VBClFatalError(("Received a size hint for too high display number %u\n",
                            (unsigned) iDisplay));
            rc = VbglR3SeamlessGetLastEvent(&Mode);
            if (RT_FAILURE(rc))
                VBClFatalError(("Failed to check seamless mode, rc=%Rrc\n", rc));
            if (Mode == VMMDev_Seamless_Disabled)
            {
                rc = VbglR3SaveVideoMode(iDisplay, cx, cy, cBPP, x, y,
                                         fEnabled);
                if (   RT_FAILURE(rc)
                    && rc != VERR_HGCM_SERVICE_NOT_FOUND
                    && rc != VERR_NOT_IMPLEMENTED /* No HGCM */)
                    VBClFatalError(("Failed to save size hint, rc=%Rrc\n", rc));
            }
            setModeX11(pState, cx, cy, cBPP, iDisplay, x, y, fEnabled,
                       fSetPosition);
        }
    }
}

/** Display magic number, start of a UUID. */
#define DISPLAYSERVICE_MAGIC 0xf0029993

/** VBoxClient service class wrapping the logic for the display service while
 *  the main VBoxClient code provides the daemon logic needed by all services.
 */
struct DISPLAYSERVICE
{
    /** The service interface. */
    struct VBCLSERVICE *pInterface;
    /** Magic number for sanity checks. */
    uint32_t magic;
    /** State related to the X server. */
    struct x11State mState;
    /** Are we initialised yet? */
    bool mfInit;
};

static const char *getPidFilePath()
{
    return ".vboxclient-display.pid";
}

static struct DISPLAYSERVICE *getClassFromInterface(struct VBCLSERVICE **
                                                         ppInterface)
{
    struct DISPLAYSERVICE *pSelf = (struct DISPLAYSERVICE *)ppInterface;
    if (pSelf->magic != DISPLAYSERVICE_MAGIC)
        VBClFatalError(("Bad display service object!\n"));
    return pSelf;
}

static int init(struct VBCLSERVICE **ppInterface)
{
    struct DISPLAYSERVICE *pSelf = getClassFromInterface(ppInterface);
    int rc;
    
    if (pSelf->mfInit)
        return VERR_WRONG_ORDER;
    rc = initX11(&pSelf->mState);
    if (RT_FAILURE(rc))
        return rc;
    rc = enableEventsAndCaps();
    if (RT_SUCCESS(rc))
        pSelf->mfInit = true;
    return rc;
}

static int run(struct VBCLSERVICE **ppInterface, bool fDaemonised)
{
    struct DISPLAYSERVICE *pSelf = getClassFromInterface(ppInterface);

    if (!pSelf->mfInit)
        return VERR_WRONG_ORDER;
    runDisplay(&pSelf->mState);
    return VERR_INTERNAL_ERROR;  /* "Should never reach here." */
}

static int pause(struct VBCLSERVICE **ppInterface)
{
    struct DISPLAYSERVICE *pSelf = getClassFromInterface(ppInterface);

    if (!pSelf->mfInit)
        return VERR_WRONG_ORDER;
    return disableEventsAndCaps();
}

static int resume(struct VBCLSERVICE **ppInterface)
{
    struct DISPLAYSERVICE *pSelf = getClassFromInterface(ppInterface);

    if (!pSelf->mfInit)
        return VERR_WRONG_ORDER;
    return enableEventsAndCaps();
}

static void cleanup(struct VBCLSERVICE **ppInterface)
{
    NOREF(ppInterface);
    disableEventsAndCaps();
}

struct VBCLSERVICE vbclDisplayInterface =
{
    getPidFilePath,
    init,
    run,
    pause,
    resume,
    cleanup    
};

struct VBCLSERVICE **VBClGetDisplayService()
{
    struct DISPLAYSERVICE *pService =
        (struct DISPLAYSERVICE *)RTMemAlloc(sizeof(*pService));

    if (!pService)
        VBClFatalError(("Out of memory\n"));
    pService->pInterface = &vbclDisplayInterface;
    pService->magic = DISPLAYSERVICE_MAGIC;
    pService->mfInit = false;
    return &pService->pInterface;
}
