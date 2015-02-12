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

/* TESTING: Dynamic resizing and mouse integration toggling should work
 * correctly with a range of X servers (pre-1.3, 1.3 and later under Linux, 1.3
 * and later under Solaris) with Guest Additions installed.  Switching to a
 * virtual terminal while a user session is in place should disable dynamic
 * resizing and cursor integration, switching back should re-enable them. */

/** Most recent information received for a particular screen. */
struct screenInformation
{
    unsigned cx;
    unsigned cy;
    unsigned cBPP;
    unsigned x;
    unsigned y;
    bool fEnabled;
    bool fUpdateSize;
    bool fUpdatePosition;
};

/** Display magic number, start of a UUID. */
#define DISPLAYSTATE_MAGIC UINT32_C(0xf0029993)

/** State information needed for the service.  The main VBoxClient code provides
 *  the daemon logic needed by all services. */
struct DISPLAYSTATE
{
    /** The service interface. */
    struct VBCLSERVICE *pInterface;
    /** Magic number for sanity checks. */
    uint32_t magic;
    /** Are we initialised yet? */
    bool mfInit;
    /** The connection to the server. */
    Display *pDisplay;
    /** Can we use version 1.2 or later of the RandR protocol here? */
    bool fHaveRandR12;
    /** The command argument to use for the xrandr binary.  Currently only
     * used to support the non-standard location on some Solaris systems -
     * would it make sense to use absolute paths on all systems? */
    const char *pcszXrandr;
    /** The number of screens we are currently aware of. */
    unsigned cScreensTracked;
    /** Array of information about different screens. */
    struct screenInformation *paScreenInformation;
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

static int initDisplay(struct DISPLAYSTATE *pState)
{
    char szCommand[256];
    int status;

    /* Initialise the guest library. */
    int rc = VbglR3InitUser();
    if (RT_FAILURE(rc))
        VBClFatalError(("Failed to connect to the VirtualBox kernel service, rc=%Rrc\n", rc));
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
    pState->cScreensTracked = 0;
    pState->paScreenInformation = NULL;
    return VINF_SUCCESS;
}

static void updateScreenInformation(struct DISPLAYSTATE *pState, unsigned cx, unsigned cy, unsigned cBPP, unsigned iDisplay,
                                    unsigned x, unsigned y, bool fEnabled, bool fUpdatePosition)
{
    uint32_t i;

    if (iDisplay >= pState->cScreensTracked)
    {
        pState->paScreenInformation =
                (struct screenInformation *)RTMemRealloc(pState->paScreenInformation,
                                                         (iDisplay + 1) * sizeof(*pState->paScreenInformation));
        if (!pState->paScreenInformation)
            VBClFatalError(("Failed to re-allocate screen information.\n"));
        for (i = pState->cScreensTracked; i < iDisplay + 1; ++i)
            RT_ZERO(pState->paScreenInformation[i]);
        pState->cScreensTracked = iDisplay + 1;
    }
    pState->paScreenInformation[iDisplay].cx = cx;
    pState->paScreenInformation[iDisplay].cy = cy;
    pState->paScreenInformation[iDisplay].cBPP = cBPP;
    pState->paScreenInformation[iDisplay].x = x;
    pState->paScreenInformation[iDisplay].y = y;
    pState->paScreenInformation[iDisplay].fEnabled = fEnabled;
    pState->paScreenInformation[iDisplay].fUpdateSize = true;
    pState->paScreenInformation[iDisplay].fUpdatePosition = fUpdatePosition;
}

static void updateSizeHintsProperty(struct DISPLAYSTATE *pState)
{
    unsigned long *paSizeHints = (unsigned long *)RTMemTmpAllocZ(pState->cScreensTracked * sizeof(unsigned long));
    unsigned i;

    if (paSizeHints == NULL)
        VBClFatalError(("Failed to allocate size hint property memory.\n"));
    for (i = 0; i < pState->cScreensTracked; ++i)
    {
        if (   pState->paScreenInformation[i].fEnabled
            && pState->paScreenInformation[i].cx != 0 && pState->paScreenInformation[i].cy != 0)
            paSizeHints[i] = (pState->paScreenInformation[i].cx & 0x8fff) << 16 | (pState->paScreenInformation[i].cy & 0x8fff);
        else if (pState->paScreenInformation[i].cx != 0 && pState->paScreenInformation[i].cy != 0)
            paSizeHints[i] = -1;
    }
    XChangeProperty(pState->pDisplay, DefaultRootWindow(pState->pDisplay), XInternAtom(pState->pDisplay, "VBOX_SIZE_HINTS", 0),
                    XA_INTEGER, 32, PropModeReplace, (unsigned char *)paSizeHints, pState->cScreensTracked);
    XFlush(pState->pDisplay);
    RTMemTmpFree(paSizeHints);
}

static void notifyXServer(struct DISPLAYSTATE *pState)
{
    char szCommand[256];
    unsigned i;
    bool fUpdateInformation = false;

    /** @note The xrandr command can fail if something else accesses RandR at
     *  the same time.  We just ignore failure for now and let the user try
     *  again as we do not know what someone else is doing. */
    for (i = 0; i < pState->cScreensTracked; ++i)
        if (pState->paScreenInformation[i].fUpdateSize)
            fUpdateInformation = true;
    if (   !pState->fHaveRandR12 && pState->paScreenInformation[0].fUpdateSize
        && pState->paScreenInformation[0].cx > 0 && pState->paScreenInformation[0].cy > 0)
    {
        RTStrPrintf(szCommand, sizeof(szCommand), "%s -s %ux%u",
                    pState->pcszXrandr, pState->paScreenInformation[0].cx, pState->paScreenInformation[0].cy);
        system(szCommand);
        pState->paScreenInformation[0].fUpdateSize = false;
    }
    else if (pState->fHaveRandR12 && fUpdateInformation)
        for (i = 0; i < pState->cScreensTracked; ++i)
        {
            if (pState->paScreenInformation[i].fUpdateSize)
            {
                RTStrPrintf(szCommand, sizeof(szCommand), "%s --output VGA-%u --preferred", pState->pcszXrandr, i);
                system(szCommand);
            }
            if (pState->paScreenInformation[i].fUpdatePosition)
            {
                RTStrPrintf(szCommand, sizeof(szCommand), "%s --output VGA-%u --auto --pos %ux%u",
                            pState->pcszXrandr, i, pState->paScreenInformation[i].x, pState->paScreenInformation[i].y);
                system(szCommand);
            }
            pState->paScreenInformation[i].fUpdateSize = pState->paScreenInformation[i].fUpdatePosition = false;
        }
    else
    {
        RTStrPrintf(szCommand, sizeof(szCommand), "%s", pState->pcszXrandr);
        system(szCommand);
    }
}

static void updateMouseCapabilities(struct DISPLAYSTATE *pState)
{
    uint32_t fFeatures = 0;
    int rc;
    unsigned i;

    rc = VbglR3GetMouseStatus(&fFeatures, NULL, NULL);
    
    if (rc != VINF_SUCCESS)
        VBClFatalError(("Failed to get mouse status, rc=%Rrc\n", rc));
    XChangeProperty(pState->pDisplay, DefaultRootWindow(pState->pDisplay),
                    XInternAtom(pState->pDisplay, "VBOX_MOUSE_CAPABILITIES", 0), XA_INTEGER, 32, PropModeReplace,
                    (unsigned char *)&fFeatures, 1);
    XFlush(pState->pDisplay);
    if (pState->fHaveRandR12)
        for (i = 0; i < pState->cScreensTracked; ++i)
            pState->paScreenInformation[i].fUpdateSize = true;
    else
        pState->paScreenInformation[0].fUpdateSize = true;
}

/**
 * Display change request monitor thread function.
 */
static void runDisplay(struct DISPLAYSTATE *pState)
{
    int status, rc;
    unsigned i, cScreensTracked;
    char szCommand[256];

    LogRelFlowFunc(("\n"));
    rc = VbglR3VideoModeGetHighestSavedScreen(&cScreensTracked);
    if (rc != VINF_SUCCESS && rc != VERR_NOT_SUPPORTED)
        VBClFatalError(("Failed to get the number of saved screen modes, rc=%Rrc\n", rc));
    /* Make sure that we have an entry for screen 1 at least. */
    updateScreenInformation(pState, 1024, 768, 0, 1, 0, 0, true, false);
    if (rc == VINF_SUCCESS)
    {
        /* The "8" is for the sanity test below. */
        for (i = 0; i < RT_MAX(cScreensTracked + 1, 8); ++i)
        {
            unsigned cx = 0, cy = 0, cBPP = 0, x = 0, y = 0;
            bool fEnabled = true;

            rc = VbglR3RetrieveVideoMode(i, &cx, &cy, &cBPP, &x, &y,
                                         &fEnabled);
            /* Sanity test for VbglR3VideoModeGetHighestSavedScreen(). */
            if (i > cScreensTracked && rc != VERR_NOT_FOUND)
                VBClFatalError(("Internal error retrieving the number of saved screen modes.\n"));
            if (rc == VINF_SUCCESS)
                updateScreenInformation(pState, cx, cy, cBPP, i, x, y, fEnabled, true);
        }
    }
    while (true)
    {
        uint32_t fEvents;
        updateMouseCapabilities(pState);
        updateSizeHintsProperty(pState);
        notifyXServer(pState);
        do
            rc = VbglR3WaitEvent(  VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST
                                 | VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED,
                                 RT_INDEFINITE_WAIT, &fEvents);
        while(rc == VERR_INTERRUPTED);
        if (RT_FAILURE(rc))  /* VERR_NO_MEMORY? */
            VBClFatalError(("event wait failed, rc=%Rrc\n", rc));
        /* If it is a size hint, set the new size. */
        if (fEvents & VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST)
        {
            uint32_t cx = 0, cy = 0, cBPP = 0, iDisplay = 0, x = 0, y = 0;
            bool fEnabled = true, fUpdatePosition = true;
            VMMDevSeamlessMode Mode;

            rc = VbglR3GetDisplayChangeRequest(&cx, &cy, &cBPP, &iDisplay,
                                               &x, &y, &fEnabled,
                                               &fUpdatePosition, true);
            if (rc != VINF_SUCCESS)
                VBClFatalError(("Failed to get display change request, rc=%Rrc\n",
                                rc));
            else
                LogRelFlowFunc(("Got size hint from host cx=%d, cy=%d, bpp=%d, iDisplay=%d, x=%d, y=%d fEnabled=%d\n",
                                cx, cy, cBPP, iDisplay, x, y, fEnabled));
            if (iDisplay > INT32_MAX)
                VBClFatalError(("Received a size hint for too high display number %u\n",
                            (unsigned) iDisplay));
            updateScreenInformation(pState, cx, cy, cBPP, iDisplay, x, y, fEnabled, fUpdatePosition);
            rc = VbglR3SeamlessGetLastEvent(&Mode);
            if (RT_FAILURE(rc))
                VBClFatalError(("Failed to check seamless mode, rc=%Rrc\n", rc));
            if (Mode == VMMDev_Seamless_Disabled)
            {
                rc = VbglR3SaveVideoMode(iDisplay, cx, cy, cBPP, x, y,
                                         fEnabled);
                if (RT_FAILURE(rc) && rc != VERR_NOT_SUPPORTED)
                    VBClFatalError(("Failed to save size hint, rc=%Rrc\n", rc));
            }
        }
    }
}

static const char *getPidFilePath()
{
    return ".vboxclient-display.pid";
}

static struct DISPLAYSTATE *getStateFromInterface(struct VBCLSERVICE **ppInterface)
{
    struct DISPLAYSTATE *pSelf = (struct DISPLAYSTATE *)ppInterface;
    if (pSelf->magic != DISPLAYSTATE_MAGIC)
        VBClFatalError(("Bad display service object!\n"));
    return pSelf;
}

static int init(struct VBCLSERVICE **ppInterface)
{
    struct DISPLAYSTATE *pSelf = getStateFromInterface(ppInterface);
    int rc;

    if (pSelf->mfInit)
        return VERR_WRONG_ORDER;
    rc = initDisplay(pSelf);
    if (RT_FAILURE(rc))
        return rc;
    rc = enableEventsAndCaps();
    if (RT_SUCCESS(rc))
        pSelf->mfInit = true;
    return rc;
}

static int run(struct VBCLSERVICE **ppInterface, bool fDaemonised)
{
    struct DISPLAYSTATE *pSelf = getStateFromInterface(ppInterface);
    int rc;

    if (!pSelf->mfInit)
        return VERR_WRONG_ORDER;
    rc = VBClStartVTMonitor();
    if (RT_FAILURE(rc))
        VBClFatalError(("Failed to start the VT monitor thread: %Rrc\n", rc));
    runDisplay(pSelf);
    return VERR_INTERNAL_ERROR;  /* "Should never reach here." */
}

static int pause(struct VBCLSERVICE **ppInterface)
{
    struct DISPLAYSTATE *pSelf = getStateFromInterface(ppInterface);

    if (!pSelf->mfInit)
        return VERR_WRONG_ORDER;
    return disableEventsAndCaps();
}

static int resume(struct VBCLSERVICE **ppInterface)
{
    struct DISPLAYSTATE *pSelf = getStateFromInterface(ppInterface);

    if (!pSelf->mfInit)
        return VERR_WRONG_ORDER;
    return enableEventsAndCaps();
}

static void cleanup(struct VBCLSERVICE **ppInterface)
{
    NOREF(ppInterface);
    disableEventsAndCaps();
    VbglR3Term();
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
    struct DISPLAYSTATE *pService = (struct DISPLAYSTATE *)RTMemAlloc(sizeof(*pService));

    if (!pService)
        VBClFatalError(("Out of memory\n"));
    pService->pInterface = &vbclDisplayInterface;
    pService->magic = DISPLAYSTATE_MAGIC;
    pService->mfInit = false;
    return &pService->pInterface;
}
