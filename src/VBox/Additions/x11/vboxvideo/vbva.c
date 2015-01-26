/** @file
 * VirtualBox X11 Additions graphics driver 2D acceleration functions
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

/* ShadowFB support */
#include "shadowfb.h"

#include "vboxvideo.h"

#ifdef XORG_7X
# include <stdlib.h>
#endif

/**************************************************************************
* Main functions                                                          *
**************************************************************************/

/**
 * Callback function called by the X server to tell us about dirty
 * rectangles in the video buffer.
 *
 * @param pScreen pointer to the information structure for the current
 *                screen
 * @param iRects  Number of dirty rectangles to update
 * @param aRects  Array of structures containing the coordinates of the
 *                rectangles
 */
static void
vboxHandleDirtyRect(ScrnInfoPtr pScrn, int iRects, BoxPtr aRects)
{
    VBVACMDHDR cmdHdr;
    VBOXPtr pVBox;
    int i;
    unsigned j;

    pVBox = pScrn->driverPrivate;
    if (!pScrn->vtSema)
        return;

    for (j = 0; j < pVBox->cScreens; ++j)
    {
        /* Just continue quietly if VBVA is not currently active. */
        struct VBVABUFFER *pVBVA = pVBox->pScreens[j].aVbvaCtx.pVBVA;
        if (   !pVBVA
            || !(pVBVA->hostFlags.u32HostEvents & VBVA_F_MODE_ENABLED))
            continue;
        for (i = 0; i < iRects; ++i)
        {
            if (   aRects[i].x1 >   pVBox->pScreens[j].aScreenLocation.x
                                  + pVBox->pScreens[j].aScreenLocation.cx
                || aRects[i].y1 >   pVBox->pScreens[j].aScreenLocation.y
                                  + pVBox->pScreens[j].aScreenLocation.cy
                || aRects[i].x2 <   pVBox->pScreens[j].aScreenLocation.x
                || aRects[i].y2 <   pVBox->pScreens[j].aScreenLocation.y)
                continue;
            cmdHdr.x = (int16_t)aRects[i].x1;
            cmdHdr.y = (int16_t)aRects[i].y1;
            cmdHdr.w = (uint16_t)(aRects[i].x2 - aRects[i].x1);
            cmdHdr.h = (uint16_t)(aRects[i].y2 - aRects[i].y1);

#if 0
            TRACE_LOG("display=%u, x=%d, y=%d, w=%d, h=%d\n",
                      j, cmdHdr.x, cmdHdr.y, cmdHdr.w, cmdHdr.h);
#endif

            if (VBoxVBVABufferBeginUpdate(&pVBox->pScreens[j].aVbvaCtx,
                                          &pVBox->guestCtx))
            {
                VBoxVBVAWrite(&pVBox->pScreens[j].aVbvaCtx, &pVBox->guestCtx, &cmdHdr,
                              sizeof(cmdHdr));
                VBoxVBVABufferEndUpdate(&pVBox->pScreens[j].aVbvaCtx);
            }
        }
    }
}

/** Callback to fill in the view structures */
static int
vboxFillViewInfo(void *pvVBox, struct VBVAINFOVIEW *pViews, uint32_t cViews)
{
    VBOXPtr pVBox = (VBOXPtr)pvVBox;
    unsigned i;
    for (i = 0; i < cViews; ++i)
    {
        pViews[i].u32ViewIndex = i;
        pViews[i].u32ViewOffset = 0;
        pViews[i].u32ViewSize = pVBox->cbView;
        pViews[i].u32MaxScreenSize = pVBox->cbFBMax;
    }
    return VINF_SUCCESS;
}

/**
 * Initialise VirtualBox's accelerated video extensions.
 *
 * @returns TRUE on success, FALSE on failure
 */
static Bool
vboxInitVbva(int scrnIndex, ScreenPtr pScreen, VBOXPtr pVBox)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    int rc = VINF_SUCCESS;

    /* Why is this here?  In case things break before we have found the real
     * count? */
    pVBox->cScreens = 1;
    if (!VBoxHGSMIIsSupported())
    {
        xf86DrvMsg(scrnIndex, X_ERROR, "The graphics device does not seem to support HGSMI.  Disableing video acceleration.\n");
        return FALSE;
    }

    /* Set up the dirty rectangle handler.  It will be added into a function
     * chain and gets removed when the screen is cleaned up. */
    if (ShadowFBInit2(pScreen, NULL, vboxHandleDirtyRect) != TRUE)
    {
        xf86DrvMsg(scrnIndex, X_ERROR,
                   "Unable to install dirty rectangle handler for VirtualBox graphics acceleration.\n");
        return FALSE;
    }
    return TRUE;
}

static DECLCALLBACK(void *) hgsmiEnvAlloc(void *pvEnv, HGSMISIZE cb)
{
    NOREF(pvEnv);
    return calloc(1, cb);
}

static DECLCALLBACK(void) hgsmiEnvFree(void *pvEnv, void *pv)
{
    NOREF(pvEnv);
    free(pv);
}

static HGSMIENV g_hgsmiEnv =
{
    NULL,
    hgsmiEnvAlloc,
    hgsmiEnvFree
};

/**
 * Initialise VirtualBox's accelerated video extensions.
 *
 * @returns TRUE on success, FALSE on failure
 */
static Bool
vboxSetupVRAMVbva(ScrnInfoPtr pScrn, VBOXPtr pVBox)
{
    int rc = VINF_SUCCESS;
    unsigned i;
    uint32_t offVRAMBaseMapping, offGuestHeapMemory, cbGuestHeapMemory;
    void *pvGuestHeapMemory;

    VBoxHGSMIGetBaseMappingInfo(pScrn->videoRam * 1024, &offVRAMBaseMapping,
                                NULL, &offGuestHeapMemory, &cbGuestHeapMemory,
                                NULL);
    pvGuestHeapMemory =   ((uint8_t *)pVBox->base) + offVRAMBaseMapping
                        + offGuestHeapMemory;
    TRACE_LOG("video RAM: %u KB, guest heap offset: 0x%x, cbGuestHeapMemory: %u\n",
              pScrn->videoRam, offVRAMBaseMapping + offGuestHeapMemory,
              cbGuestHeapMemory);
    rc = VBoxHGSMISetupGuestContext(&pVBox->guestCtx, pvGuestHeapMemory,
                                    cbGuestHeapMemory,
                                    offVRAMBaseMapping + offGuestHeapMemory,
                                    &g_hgsmiEnv);
    if (RT_FAILURE(rc))
    {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Failed to set up the guest-to-host communication context, rc=%d\n", rc);
        return FALSE;
    }
    pVBox->cbView = pVBox->cbFBMax = offVRAMBaseMapping;
    pVBox->cScreens = VBoxHGSMIGetMonitorCount(&pVBox->guestCtx);
    pVBox->pScreens = calloc(pVBox->cScreens, sizeof(*pVBox->pScreens));
    if (pVBox->pScreens == NULL)
        FatalError("Failed to allocate memory for screens array.\n");
#ifdef VBOXVIDEO_13
    pVBox->paVBVAModeHints = calloc(pVBox->cScreens,
                                    sizeof(*pVBox->paVBVAModeHints));
    if (pVBox->paVBVAModeHints == NULL)
        FatalError("Failed to allocate memory for mode hints array.\n");
#endif
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Requested monitor count: %u\n",
               pVBox->cScreens);
    for (i = 0; i < pVBox->cScreens; ++i)
    {
        pVBox->cbFBMax -= VBVA_MIN_BUFFER_SIZE;
        pVBox->pScreens[i].aoffVBVABuffer = pVBox->cbFBMax;
        TRACE_LOG("VBVA buffer offset for screen %u: 0x%lx\n", i,
                  (unsigned long) pVBox->cbFBMax);
        VBoxVBVASetupBufferContext(&pVBox->pScreens[i].aVbvaCtx,
                                   pVBox->pScreens[i].aoffVBVABuffer,
                                   VBVA_MIN_BUFFER_SIZE);
    }
    TRACE_LOG("Maximum framebuffer size: %lu (0x%lx)\n",
              (unsigned long) pVBox->cbFBMax,
              (unsigned long) pVBox->cbFBMax);
    rc = VBoxHGSMISendViewInfo(&pVBox->guestCtx, pVBox->cScreens,
                               vboxFillViewInfo, (void *)pVBox);
    if (RT_FAILURE(rc))
    {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Failed to send the view information to the host, rc=%d\n", rc);
        return FALSE;
    }
    return TRUE;
}

void
vbox_open(ScrnInfoPtr pScrn, ScreenPtr pScreen, VBOXPtr pVBox)
{
    TRACE_ENTRY();

    if (!vboxInitVbva(pScrn->scrnIndex, pScreen, pVBox))
        FatalError("failed to initialise vboxvideo graphics acceleration.\n");
}

/**
 * Inform VBox that we will supply it with dirty rectangle information
 * and install the dirty rectangle handler.
 *
 * @returns TRUE for success, FALSE for failure
 * @param   pScrn   Pointer to a structure describing the X screen in use
 */
Bool
vboxEnableVbva(ScrnInfoPtr pScrn)
{
    bool rc = TRUE;
    int scrnIndex = pScrn->scrnIndex;
    unsigned i;
    VBOXPtr pVBox = pScrn->driverPrivate;

    TRACE_ENTRY();
    if (!vboxSetupVRAMVbva(pScrn, pVBox))
        return FALSE;
    for (i = 0; i < pVBox->cScreens; ++i)
    {
        struct VBVABUFFER *pVBVA;

        pVBVA = (struct VBVABUFFER *) (  ((uint8_t *)pVBox->base)
                                       + pVBox->pScreens[i].aoffVBVABuffer);
        if (!VBoxVBVAEnable(&pVBox->pScreens[i].aVbvaCtx, &pVBox->guestCtx,
                            pVBVA, i))
            rc = FALSE;
    }
    if (!rc)
    {
        /* Request not accepted - disable for old hosts. */
        xf86DrvMsg(scrnIndex, X_ERROR,
                   "Failed to enable screen update reporting for at least one virtual monitor.\n");
         vboxDisableVbva(pScrn);
    }
#ifdef VBOXVIDEO_13
# ifdef RT_OS_LINUX
    if (rc && pVBox->hACPIEventHandler != NULL)
        /* We ignore the return value as the fall-back should be active
         * anyway. */
        VBoxHGSMISendCapsInfo(&pVBox->guestCtx, VBVACAPS_VIDEO_MODE_HINTS | VBVACAPS_DISABLE_CURSOR_INTEGRATION);
# endif
#endif
    return rc;
}

/**
 * Inform VBox that we will stop supplying it with dirty rectangle
 * information. This function is intended to be called when an X
 * virtual terminal is disabled, or the X server is terminated.
 *
 * @returns TRUE for success, FALSE for failure
 * @param   pScrn   Pointer to a structure describing the X screen in use
 */
void
vboxDisableVbva(ScrnInfoPtr pScrn)
{
    int rc;
    int scrnIndex = pScrn->scrnIndex;
    unsigned i;
    VBOXPtr pVBox = pScrn->driverPrivate;

    TRACE_ENTRY();
    for (i = 0; i < pVBox->cScreens; ++i)
        VBoxVBVADisable(&pVBox->pScreens[i].aVbvaCtx, &pVBox->guestCtx, i);
}
