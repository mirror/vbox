/** @file
 * VirtualBox X11 Additions graphics driver utility functions
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <VBox/VMMDev.h>
#include <VBox/VBoxGuestLib.h>

#ifndef PCIACCESS
# include <xf86Pci.h>
# include <Pci.h>
#endif

#include "xf86.h"
#define NEED_XF86_TYPES
#include <iprt/string.h>
#include "compiler.h"

#include "vboxvideo.h"

#ifdef XORG_7X
# include <stdio.h>
# include <stdlib.h>
#endif

/**************************************************************************
* Main functions                                                          *
**************************************************************************/

/**
 * Inform VBox that we are aware of advanced graphics functions
 * (i.e. dynamic resizing, seamless).
 *
 * @returns TRUE for success, FALSE for failure
 */
Bool
vboxEnableGraphicsCap(VBOXPtr pVBox)
{
    TRACE_ENTRY();
    if (!pVBox->useDevice)
        return FALSE;
    return RT_SUCCESS(VbglR3SetGuestCaps(VMMDEV_GUEST_SUPPORTS_GRAPHICS, 0));
}

/**
 * Inform VBox that we are no longer aware of advanced graphics functions
 * (i.e. dynamic resizing, seamless).
 *
 * @returns TRUE for success, FALSE for failure
 */
Bool
vboxDisableGraphicsCap(VBOXPtr pVBox)
{
    TRACE_ENTRY();
    if (!pVBox->useDevice)
        return FALSE;
    return RT_SUCCESS(VbglR3SetGuestCaps(0, VMMDEV_GUEST_SUPPORTS_GRAPHICS));
}

/**
 * Query the last display change request.
 *
 * @returns boolean success indicator.
 * @param   pScrn       Pointer to the X screen info structure.
 * @param   pcx         Where to store the horizontal pixel resolution (0 = do not change).
 * @param   pcy         Where to store the vertical pixel resolution (0 = do not change).
 * @param   pcBits      Where to store the bits per pixel (0 = do not change).
 * @param   iDisplay    Where to store the display number the request was for - 0 for the
 *                      primary display, 1 for the first secondary, etc.
 */
Bool
vboxGetDisplayChangeRequest(ScrnInfoPtr pScrn, uint32_t *pcx, uint32_t *pcy,
                            uint32_t *pcBits, uint32_t *piDisplay)
{
    VBOXPtr pVBox = pScrn->driverPrivate;
    TRACE_ENTRY();
    if (!pVBox->useDevice)
        return FALSE;
    int rc = VbglR3GetDisplayChangeRequest(pcx, pcy, pcBits, piDisplay, false);
    if (RT_SUCCESS(rc))
        return TRUE;
    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Failed to obtain the last resolution requested by the guest, rc=%d.\n", rc);
    return FALSE;
}


/**
 * Query the host as to whether it likes a specific video mode.
 *
 * @returns the result of the query
 * @param   cx     the width of the mode being queried
 * @param   cy     the height of the mode being queried
 * @param   cBits  the bpp of the mode being queried
 */
Bool
vboxHostLikesVideoMode(ScrnInfoPtr pScrn, uint32_t cx, uint32_t cy, uint32_t cBits)
{
    VBOXPtr pVBox = pScrn->driverPrivate;
    TRACE_ENTRY();
    if (!pVBox->useDevice)
        return TRUE;  /* If we can't ask the host then we like everything. */
    return VbglR3HostLikesVideoMode(cx, cy, cBits);
}

/**
 * Check if any seamless mode is enabled.
 * Seamless is only relevant for the newer Xorg modules.
 *
 * @returns the result of the query
 * (true = seamless enabled, false = seamless not enabled)
 * @param   pScrn  Screen info pointer.
 */
Bool
vboxGuestIsSeamless(ScrnInfoPtr pScrn)
{
    VMMDevSeamlessMode mode;
    VBOXPtr pVBox = pScrn->driverPrivate;
    TRACE_ENTRY();
    if (!pVBox->useDevice)
        return FALSE;
    if (RT_FAILURE(VbglR3SeamlessGetLastEvent(&mode)))
        return FALSE;
    return (mode != VMMDev_Seamless_Disabled);
}

/**
 * Save video mode parameters to the registry.
 *
 * @returns iprt status value
 * @param   pszName the name to save the mode parameters under
 * @param   cx      mode width
 * @param   cy      mode height
 * @param   cBits   bits per pixel for the mode
 */
Bool
vboxSaveVideoMode(ScrnInfoPtr pScrn, uint32_t cx, uint32_t cy, uint32_t cBits)
{
    VBOXPtr pVBox = pScrn->driverPrivate;
    TRACE_ENTRY();
    if (!pVBox->useDevice)
        return FALSE;
    return RT_SUCCESS(VbglR3SaveVideoMode("SavedMode", cx, cy, cBits));
}

/**
 * Retrieve video mode parameters from the registry.
 *
 * @returns iprt status value
 * @param   pszName the name under which the mode parameters are saved
 * @param   pcx     where to store the mode width
 * @param   pcy     where to store the mode height
 * @param   pcBits  where to store the bits per pixel for the mode
 */
Bool
vboxRetrieveVideoMode(ScrnInfoPtr pScrn, uint32_t *pcx, uint32_t *pcy, uint32_t *pcBits)
{
    VBOXPtr pVBox = pScrn->driverPrivate;
    int rc;
    TRACE_ENTRY();
    if (!pVBox->useDevice)
        rc = VERR_NOT_AVAILABLE;
    else
        rc = VbglR3RetrieveVideoMode("SavedMode", pcx, pcy, pcBits);
    if (RT_SUCCESS(rc))
        TRACE_LOG("Retrieved a video mode of %dx%dx%d\n", *pcx, *pcy, *pcBits);
    else
        TRACE_LOG("Failed to retrieve video mode, error %d\n", rc);
    return (RT_SUCCESS(rc));
}

/**
 * Fills a display mode M with a built-in mode of name pszName and dimensions
 * cx and cy.
 */
static void vboxFillDisplayMode(ScrnInfoPtr pScrn, DisplayModePtr m,
                                const char *pszName, unsigned cx, unsigned cy)
{
    VBOXPtr pVBox = pScrn->driverPrivate;
    TRACE_LOG("pszName=%s, cx=%u, cy=%u\n", pszName, cx, cy);
    m->status        = MODE_OK;
    m->type          = M_T_BUILTIN;
    /* Older versions of VBox only support screen widths which are a multiple
     * of 8 */
    if (pVBox->fAnyX)
        m->HDisplay  = cx;
    else
        m->HDisplay  = cx & ~7;
    m->HSyncStart    = m->HDisplay + 2;
    m->HSyncEnd      = m->HDisplay + 4;
    m->HTotal        = m->HDisplay + 6;
    m->VDisplay      = cy;
    m->VSyncStart    = m->VDisplay + 2;
    m->VSyncEnd      = m->VDisplay + 4;
    m->VTotal        = m->VDisplay + 6;
    m->Clock         = m->HTotal * m->VTotal * 60 / 1000; /* kHz */
    if (pszName)
    {
        if (m->name)
            free(m->name);
        m->name      = xnfstrdup(pszName);
    }
}

/** vboxvideo's list of standard video modes */
struct
{
    /** mode width */
    uint32_t cx;
    /** mode height */
    uint32_t cy;
} vboxStandardModes[] =
{
    { 1600, 1200 },
    { 1440, 1050 },
    { 1280, 960 },
    { 1024, 768 },
    { 800, 600 },
    { 640, 480 },
    { 0, 0 }
};
enum
{
    vboxNumStdModes = sizeof(vboxStandardModes) / sizeof(vboxStandardModes[0])
};

/**
 * Returns a standard mode which the host likes.  Can be called multiple
 * times with the index returned by the previous call to get a list of modes.
 * @returns  the index of the mode in the list, or 0 if no more modes are
 *           available
 * @param    pScrn   the screen information structure
 * @param    pScrn->bitsPerPixel
 *                   if this is non-null, only modes with this BPP will be
 *                   returned
 * @param    cIndex  the index of the last mode queried, or 0 to query the
 *                   first mode available.  Note: the first index is 1
 * @param    pcx     where to store the mode's width
 * @param    pcy     where to store the mode's height
 * @param    pcBits  where to store the mode's BPP
 */
unsigned vboxNextStandardMode(ScrnInfoPtr pScrn, unsigned cIndex,
                              uint32_t *pcx, uint32_t *pcy,
                              uint32_t *pcBits)
{
    XF86ASSERT(cIndex < vboxNumStdModes,
               ("cIndex = %d, vboxNumStdModes = %d\n", cIndex,
                vboxNumStdModes));
    for (unsigned i = cIndex; i < vboxNumStdModes - 1; ++i)
    {
        uint32_t cBits = pScrn->bitsPerPixel;
        uint32_t cx = vboxStandardModes[i].cx;
        uint32_t cy = vboxStandardModes[i].cy;

        if (cBits != 0 && !vboxHostLikesVideoMode(pScrn, cx, cy, cBits))
            continue;
        if (vboxHostLikesVideoMode(pScrn, cx, cy, 32))
            cBits = 32;
        else if (vboxHostLikesVideoMode(pScrn, cx, cy, 16))
            cBits = 16;
        else
            continue;
        if (pcx)
            *pcx = cx;
        if (pcy)
            *pcy = cy;
        if (pcBits)
            *pcBits = cBits;
        return i + 1;
    }
    return 0;
}

/**
 * Returns the preferred video mode.  The current order of preference is
 * (from highest to least preferred):
 *  - The mode corresponding to the last size hint from the host
 *  - The video mode saved from the last session
 *  - The largest standard mode which the host likes, falling back to
 *    640x480x32 as a worst case
 *  - If the host can't be contacted at all, we return 1024x768x32
 *
 * The return type is void as we guarantee we will return some mode.
 */
void vboxGetPreferredMode(ScrnInfoPtr pScrn, uint32_t iScreen, uint32_t *pcx,
                          uint32_t *pcy, uint32_t *pcBits)
{
    /* Query the host for the preferred resolution and colour depth */
    uint32_t cx = 0, cy = 0, iScreenIn = iScreen, cBits = 32;
    VBOXPtr pVBox = pScrn->driverPrivate;

    TRACE_LOG("iScreen=%u\n", iScreen);
    bool found = false;
    if (   pVBox->aPreferredSize[iScreen].cx
        && pVBox->aPreferredSize[iScreen].cy)
    {
        cx = pVBox->aPreferredSize[iScreen].cx;
        cy = pVBox->aPreferredSize[iScreen].cy;
        found = true;
    }
    if (pVBox->useDevice)
    {
        if (!found)
            found = vboxGetDisplayChangeRequest(pScrn, &cx, &cy, &cBits,
                                                &iScreenIn);
        if ((cx == 0) || (cy == 0) || iScreenIn != iScreen)
            found = false;
        if (!found)
            found = vboxRetrieveVideoMode(pScrn, &cx, &cy, &cBits);
        if ((cx == 0) || (cy == 0))
            found = false;
        if (!found)
            found = (vboxNextStandardMode(pScrn, 0, &cx, &cy, &cBits) != 0);
        if (!found)
        {
            /* Last resort */
            cx = 640;
            cy = 480;
            cBits = 32;
        }
    }
    else
    {
        cx = 1024;
        cy = 768;
    }
    if (pcx)
        *pcx = cx;
    if (pcy)
        *pcy = cy;
    if (pcBits)
        *pcBits = cBits;
    TRACE_LOG("cx=%u, cy=%u, cBits=%u\n", cx, cy, cBits);
}

/* Move a screen mode found to the end of the list, so that RandR will give
 * it the highest priority when a mode switch is requested.  Returns the mode
 * that was previously before the mode in the list in order to allow the
 * caller to continue walking the list. */
static DisplayModePtr vboxMoveModeToFront(ScrnInfoPtr pScrn,
                                          DisplayModePtr pMode)
{
    DisplayModePtr pPrev = pMode->prev;
    if (pMode != pScrn->modes)
    {
        pMode->prev->next = pMode->next;
        pMode->next->prev = pMode->prev;
        pMode->next = pScrn->modes;
        pMode->prev = pScrn->modes->prev;
        pMode->next->prev = pMode;
        pMode->prev->next = pMode;
        pScrn->modes = pMode;
    }
    return pPrev;
}

/**
 * Rewrites the first dynamic mode found which is not the current screen mode
 * to contain the host's currently preferred screen size, then moves that
 * mode to the front of the screen information structure's mode list.
 * Additionally, if the current mode is not dynamic, the second dynamic mode
 * will be set to match the current mode and also added to the front.  This
 * ensures that the user can always reset the current size to kick the driver
 * to update its mode list.
 */
void vboxWriteHostModes(ScrnInfoPtr pScrn, DisplayModePtr pCurrent)
{
    uint32_t cx = 0, cy = 0, iDisplay = 0, cBits = 0;
    DisplayModePtr pMode;
    bool found = false;

    TRACE_ENTRY();
    vboxGetPreferredMode(pScrn, 0, &cx, &cy, &cBits);
#ifdef DEBUG
    /* Count the number of modes for sanity */
    unsigned cModes = 1, cMode = 0;
    DisplayModePtr pCount;
    for (pCount = pScrn->modes; ; pCount = pCount->next, ++cModes)
        if (pCount->next == pScrn->modes)
            break;
#endif
    for (pMode = pScrn->modes; ; pMode = pMode->next)
    {
#ifdef DEBUG
        XF86ASSERT (cMode++ < cModes, (NULL));
#endif
        if (   pMode != pCurrent
            && !strcmp(pMode->name, "VBoxDynamicMode"))
        {
            if (!found)
                vboxFillDisplayMode(pScrn, pMode, NULL, cx, cy);
            else if (pCurrent)
                vboxFillDisplayMode(pScrn, pMode, NULL, pCurrent->HDisplay,
                                    pCurrent->VDisplay);
            found = true;
            pMode = vboxMoveModeToFront(pScrn, pMode);
        }
        if (pMode->next == pScrn->modes)
            break;
    }
    XF86ASSERT (found,
                ("vboxvideo: no free dynamic mode found.  Exiting.\n"));
    XF86ASSERT (   (pScrn->modes->HDisplay == (long) cx)
                || (   (pScrn->modes->HDisplay == pCurrent->HDisplay)
                    && (pScrn->modes->next->HDisplay == (long) cx)),
                ("pScrn->modes->HDisplay=%u, pScrn->modes->next->HDisplay=%u\n",
                 pScrn->modes->HDisplay, pScrn->modes->next->HDisplay));
    XF86ASSERT (   (pScrn->modes->VDisplay == (long) cy)
                || (   (pScrn->modes->VDisplay == pCurrent->VDisplay)
                    && (pScrn->modes->next->VDisplay == (long) cy)),
                ("pScrn->modes->VDisplay=%u, pScrn->modes->next->VDisplay=%u\n",
                 pScrn->modes->VDisplay, pScrn->modes->next->VDisplay));
}

/**
 * Allocates an empty display mode and links it into the doubly linked list of
 * modes pointed to by pScrn->modes.  Returns a pointer to the newly allocated
 * memory.
 */
static DisplayModePtr vboxAddEmptyScreenMode(ScrnInfoPtr pScrn)
{
    DisplayModePtr pMode = xnfcalloc(sizeof(DisplayModeRec), 1);

    TRACE_ENTRY();
    if (!pScrn->modes)
    {
        pScrn->modes = pMode;
        pMode->next = pMode;
        pMode->prev = pMode;
    }
    else
    {
        pMode->next = pScrn->modes;
        pMode->prev = pScrn->modes->prev;
        pMode->next->prev = pMode;
        pMode->prev->next = pMode;
    }
    return pMode;
}

/**
 * Create display mode entries in the screen information structure for each
 * of the initial graphics modes that we wish to support.  This includes:
 *  - An initial mode, of the size requested by the caller
 *  - Two dynamic modes, one of which will be updated to match the last size
 *    hint from the host on each mode switch, but initially also of the
 *    requested size
 *  - Several standard modes, if possible ones that the host likes
 *  - Any modes that the user requested in xorg.conf/XFree86Config
 */
void vboxAddModes(ScrnInfoPtr pScrn, uint32_t cxInit, uint32_t cyInit)
{
    unsigned cx = 0, cy = 0, cIndex = 0;
    /* For reasons related to the way RandR 1.1 is implemented, we need to
     * make sure that the initial mode (more precisely, a mode equal to the
     * initial virtual resolution) is always present in the mode list.  RandR
     * has the assumption build in that there will either be a mode of that
     * size present at all times, or that the first mode in the list will
     * always be smaller than the initial virtual resolution.  Since our
     * approach to dynamic resizing isn't quite the way RandR was intended to
     * be, and breaks the second assumption, we guarantee the first. */
    DisplayModePtr pMode = vboxAddEmptyScreenMode(pScrn);
    vboxFillDisplayMode(pScrn, pMode, "VBoxInitialMode", cxInit, cyInit);
    /* Create our two dynamic modes. */
    pMode = vboxAddEmptyScreenMode(pScrn);
    vboxFillDisplayMode(pScrn, pMode, "VBoxDynamicMode", cxInit, cyInit);
    pMode = vboxAddEmptyScreenMode(pScrn);
    vboxFillDisplayMode(pScrn, pMode, "VBoxDynamicMode", cxInit, cyInit);
    /* Add standard modes supported by the host */
    for ( ; ; )
    {
        char szName[256];
        cIndex = vboxNextStandardMode(pScrn, cIndex, &cx, &cy, NULL);
        if (cIndex == 0)
            break;
        sprintf(szName, "VBox-%ux%u", cx, cy);
        pMode = vboxAddEmptyScreenMode(pScrn);
        vboxFillDisplayMode(pScrn, pMode, szName, cx, cy);
    }
    /* And finally any modes specified by the user.  We assume here that
     * the mode names reflect the mode sizes. */
    for (unsigned i = 0;    pScrn->display->modes != NULL
                         && pScrn->display->modes[i] != NULL; i++)
    {
        if (sscanf(pScrn->display->modes[i], "%ux%u", &cx, &cy) == 2)
        {
            pMode = vboxAddEmptyScreenMode(pScrn);
            vboxFillDisplayMode(pScrn, pMode, pScrn->display->modes[i], cx, cy);
        }
    }
}
