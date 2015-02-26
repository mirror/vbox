/* $Id$ */
/** @file
 * VirtualBox X11 Additions graphics driver dynamic video mode functions.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "vboxvideo.h"
#include <VBox/VMMDev.h>

#define NEED_XF86_TYPES
#include <iprt/string.h>

#include "xf86.h"
#include "dixstruct.h"
#ifdef VBOX_GUESTR3XF86MOD
# define EXTENSION_PROC_ARGS char *name, GCPtr pGC
#endif
#include "extnsionst.h"
#include "windowstr.h"
#include <X11/extensions/randrproto.h>

#ifdef XORG_7X
# include <stdio.h>
# include <stdlib.h>
#endif

/**************************************************************************
* Main functions                                                          *
**************************************************************************/

/**
 * Fills a display mode M with a built-in mode of name pszName and dimensions
 * cx and cy.
 */
static void vboxFillDisplayMode(ScrnInfoPtr pScrn, DisplayModePtr m,
                                const char *pszName, unsigned cx, unsigned cy)
{
    VBOXPtr pVBox = pScrn->driverPrivate;
    char szName[256];
    DisplayModePtr pPrev = m->prev;
    DisplayModePtr pNext = m->next;

    if (!pszName)
    {
        sprintf(szName, "%ux%u", cx, cy);
        pszName = szName;
    }
    TRACE_LOG("pszName=%s, cx=%u, cy=%u\n", pszName, cx, cy);
    if (m->name)
        free((void*)m->name);
    memset(m, '\0', sizeof(*m));
    m->prev          = pPrev;
    m->next          = pNext;
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
    m->name      = xnfstrdup(pszName);
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
 * of the graphics modes that we wish to support, that is:
 *  - A dynamic mode in first place which will be updated by the RandR code.
 *  - Any modes that the user requested in xorg.conf/XFree86Config.
 */
void vboxAddModes(ScrnInfoPtr pScrn)
{
    unsigned cx = 0, cy = 0, cIndex = 0;
    unsigned i;
    DisplayModePtr pMode;

    /* Add two dynamic mode entries.  When we receive a new size hint we will
     * update whichever of these is not current. */
    pMode = vboxAddEmptyScreenMode(pScrn);
    vboxFillDisplayMode(pScrn, pMode, NULL, 1024, 768);
    pMode = vboxAddEmptyScreenMode(pScrn);
    vboxFillDisplayMode(pScrn, pMode, NULL, 1024, 768);
    /* Add any modes specified by the user.  We assume here that the mode names
     * reflect the mode sizes. */
    for (i = 0; pScrn->display->modes && pScrn->display->modes[i]; i++)
    {
        if (sscanf(pScrn->display->modes[i], "%ux%u", &cx, &cy) == 2)
        {
            pMode = vboxAddEmptyScreenMode(pScrn);
            vboxFillDisplayMode(pScrn, pMode, pScrn->display->modes[i], cx, cy);
        }
    }
}

/** Set the initial values for the guest screen size hints to standard values
 * in case nothing else is available. */
void VBoxInitialiseSizeHints(ScrnInfoPtr pScrn)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    DisplayModePtr pMode;
    unsigned i;

    for (i = 0; i < pVBox->cScreens; ++i)
    {
        pVBox->pScreens[i].aPreferredSize.cx = 1024;
        pVBox->pScreens[i].aPreferredSize.cy = 768;
        pVBox->pScreens[i].afConnected       = true;
    }
    /* Set up the first mode correctly to match the requested initial mode. */
    pScrn->modes->HDisplay = pVBox->pScreens[0].aPreferredSize.cx;
    pScrn->modes->VDisplay = pVBox->pScreens[0].aPreferredSize.cy;
}

static bool useHardwareCursor(uint32_t fCursorCapabilities)
{
    if (   !(fCursorCapabilities & VMMDEV_MOUSE_HOST_CANNOT_HWPOINTER)
        && (fCursorCapabilities & VMMDEV_MOUSE_HOST_WANTS_ABSOLUTE))
        return true;
    return false;
}

static void compareAndMaybeSetUseHardwareCursor(VBOXPtr pVBox, uint32_t fCursorCapabilities, bool *pfChanged, bool fSet)
{
    if (pVBox->fUseHardwareCursor != useHardwareCursor(fCursorCapabilities))
        *pfChanged = true;
    if (fSet)
        pVBox->fUseHardwareCursor = useHardwareCursor(fCursorCapabilities);
}

#define SIZE_HINTS_PROPERTY          "VBOX_SIZE_HINTS"
#define SIZE_HINTS_MISMATCH_PROPERTY "VBOX_SIZE_HINTS_MISMATCH"
#define MOUSE_CAPABILITIES_PROPERTY  "VBOX_MOUSE_CAPABILITIES"

#define COMPARE_AND_MAYBE_SET(pDest, src, pfChanged, fSet) \
do { \
    if (*(pDest) != (src)) \
    { \
        if (fSet) \
            *(pDest) = (src); \
        *(pfChanged) = true; \
    } \
} while(0)

/** Read in information about the most recent size hints and cursor
 * capabilities requested for the guest screens from a root window property set
 * by an X11 client.  Information obtained via HGSMI takes priority. */
void vbvxReadSizesAndCursorIntegrationFromProperties(ScrnInfoPtr pScrn, bool *pfNeedUpdate)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    size_t cModesFromProperty, cDummy;
    int32_t *paModeHints,  *pfCursorCapabilities;
    int rc;
    unsigned i;
    bool fChanged = false;
    int32_t fSizeMismatch;

    if (vbvxGetIntegerPropery(pScrn, SIZE_HINTS_PROPERTY, &cModesFromProperty, &paModeHints) != VINF_SUCCESS)
        paModeHints = NULL;
    if (paModeHints != NULL)
        for (i = 0; i < cModesFromProperty && i < pVBox->cScreens; ++i)
            if (paModeHints[i] != 0)
            {
                if (paModeHints[i] == -1)
                    COMPARE_AND_MAYBE_SET(&pVBox->pScreens[i].afConnected, false, &fChanged, !pVBox->fHaveHGSMIModeHints);
                else
                {
                    COMPARE_AND_MAYBE_SET(&pVBox->pScreens[i].aPreferredSize.cx, (paModeHints[i] >> 16) & 0x8fff, &fChanged,
                                          !pVBox->fHaveHGSMIModeHints);
                    COMPARE_AND_MAYBE_SET(&pVBox->pScreens[i].aPreferredSize.cy, paModeHints[i] & 0x8fff, &fChanged,
                                          !pVBox->fHaveHGSMIModeHints);
                    COMPARE_AND_MAYBE_SET(&pVBox->pScreens[i].afConnected, true, &fChanged, !pVBox->fHaveHGSMIModeHints);
                }
            }
    if (   vbvxGetIntegerPropery(pScrn, MOUSE_CAPABILITIES_PROPERTY, &cDummy, &pfCursorCapabilities) == VINF_SUCCESS
        && cDummy == 1)
        compareAndMaybeSetUseHardwareCursor(pVBox, *pfCursorCapabilities, &fChanged, !pVBox->fHaveHGSMIModeHints);
    fSizeMismatch = pVBox->fHaveHGSMIModeHints && fChanged;
    vbvxSetIntegerPropery(pScrn, SIZE_HINTS_MISMATCH_PROPERTY, 1, &fSizeMismatch, false);
    if (pfNeedUpdate != NULL)
        *pfNeedUpdate = !pVBox->fHaveHGSMIModeHints && fChanged;
}

/** Read in information about the most recent size hints and cursor
 * capabilities requested for the guest screens from HGSMI. */
void vbvxReadSizesAndCursorIntegrationFromHGSMI(ScrnInfoPtr pScrn, bool *pfNeedUpdate)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    int rc;
    unsigned i;
    bool fChanged = false;
    uint32_t fCursorCapabilities;

    if (pfNeedUpdate != NULL)
        *pfNeedUpdate = false;
    if (!pVBox->fHaveHGSMIModeHints)
        return;
    rc = VBoxHGSMIGetModeHints(&pVBox->guestCtx, pVBox->cScreens, pVBox->paVBVAModeHints);
    VBVXASSERT(rc == VINF_SUCCESS, ("VBoxHGSMIGetModeHints failed, rc=%d.\n", rc));
    for (i = 0; i < pVBox->cScreens; ++i)
        if (pVBox->paVBVAModeHints[i].magic == VBVAMODEHINT_MAGIC)
        {
            COMPARE_AND_MAYBE_SET(&pVBox->pScreens[i].aPreferredSize.cx, pVBox->paVBVAModeHints[i].cx & 0x8fff, &fChanged, true);
            COMPARE_AND_MAYBE_SET(&pVBox->pScreens[i].aPreferredSize.cy, pVBox->paVBVAModeHints[i].cy & 0x8fff, &fChanged, true);
            COMPARE_AND_MAYBE_SET(&pVBox->pScreens[i].afConnected, RT_BOOL(pVBox->paVBVAModeHints[i].fEnabled), &fChanged, true);
        }
    rc = VBoxQueryConfHGSMI(&pVBox->guestCtx, VBOX_VBVA_CONF32_CURSOR_CAPABILITIES, &fCursorCapabilities);
    VBVXASSERT(rc == VINF_SUCCESS, ("Getting VBOX_VBVA_CONF32_CURSOR_CAPABILITIES failed, rc=%d.\n", rc));
    compareAndMaybeSetUseHardwareCursor(pVBox, fCursorCapabilities, &fChanged, true);
    if (pfNeedUpdate != NULL)
        *pfNeedUpdate = fChanged;
}

#undef COMPARE_AND_MAYBE_SET
