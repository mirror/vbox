/** @file
 * VirtualBox X11 Additions graphics driver 2D acceleration functions
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

/* ShadowFB support */
#include "shadowfb.h"

#include "vboxvideo.h"

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
    if (pVBox->fHaveHGSMI == FALSE || pVBox->vtSwitch)
        return;

    for (i = 0; i < iRects; ++i)
        for (j = 0; j < pVBox->cScreens; ++j)
        {
            /* Just continue quietly if VBVA is not currently active. */
            struct VBVABUFFER *pVBVA = pVBox->aVbvaCtx[j].pVBVA;
            if (   !pVBVA
                || !(pVBVA->hostFlags.u32HostEvents & VBVA_F_MODE_ENABLED))
                continue;
            if (   aRects[i].x1 >   pVBox->aScreenLocation[j].x
                                  + pVBox->aScreenLocation[j].cx
                || aRects[i].y1 >   pVBox->aScreenLocation[j].y
                                  + pVBox->aScreenLocation[j].cy
                || aRects[i].x2 <   pVBox->aScreenLocation[j].x
                || aRects[i].y2 <   pVBox->aScreenLocation[j].y)
                continue;
            cmdHdr.x = (int16_t)aRects[i].x1;
            cmdHdr.y = (int16_t)aRects[i].y1;
            cmdHdr.w = (uint16_t)(aRects[i].x2 - aRects[i].x1);
            cmdHdr.h = (uint16_t)(aRects[i].y2 - aRects[i].y1);

#if 0
            TRACE_LOG("display=%u, x=%d, y=%d, w=%d, h=%d\n",
                      j, cmdHdr.x, cmdHdr.y, cmdHdr.w, cmdHdr.h);
#endif

            if (VBoxVBVABufferBeginUpdate(&pVBox->aVbvaCtx[j],
                                          &pVBox->guestCtx))
            {
                VBoxVBVAWrite(&pVBox->aVbvaCtx[j], &pVBox->guestCtx, &cmdHdr,
                              sizeof(cmdHdr));
                VBoxVBVABufferEndUpdate(&pVBox->aVbvaCtx[j]);
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

    if (!pVBox->fHaveHGSMI)
        return FALSE;
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
                                    offVRAMBaseMapping + offGuestHeapMemory);
    if (RT_FAILURE(rc))
    {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Failed to set up the guest-to-host communication context, rc=%d\n", rc);
        return FALSE;
    }
    pVBox->cbView = pVBox->cbFBMax = offVRAMBaseMapping;
    pVBox->cScreens = VBoxHGSMIGetMonitorCount(&pVBox->guestCtx);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Requested monitor count: %u\n",
               pVBox->cScreens);
    for (i = 0; i < pVBox->cScreens; ++i)
    {
        pVBox->cbFBMax -= VBVA_MIN_BUFFER_SIZE;
        pVBox->aoffVBVABuffer[i] = pVBox->cbFBMax;
        TRACE_LOG("VBVA buffer offset for screen %u: 0x%lx\n", i,
                  (unsigned long) pVBox->cbFBMax);
        VBoxVBVASetupBufferContext(&pVBox->aVbvaCtx[i],
                                   pVBox->aoffVBVABuffer[i],
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

Bool
vbox_open(ScrnInfoPtr pScrn, ScreenPtr pScreen, VBOXPtr pVBox)
{
    TRACE_ENTRY();

    pVBox->fHaveHGSMI = vboxInitVbva(pScrn->scrnIndex, pScreen, pVBox);
    return pVBox->fHaveHGSMI;
}

Bool
vbox_device_available(VBOXPtr pVBox)
{
    return pVBox->useDevice;
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
                                       + pVBox->aoffVBVABuffer[i]);
        if (!VBoxVBVAEnable(&pVBox->aVbvaCtx[i], &pVBox->guestCtx, pVBVA, i))
            rc = FALSE;
    }
    if (!rc)
    {
        /* Request not accepted - disable for old hosts. */
        xf86DrvMsg(scrnIndex, X_ERROR,
                   "Failed to enable screen update reporting for at least one virtual monitor.\n");
         vboxDisableVbva(pScrn);
    }
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
    if (!pVBox->fHaveHGSMI)  /* Ths function should not have been called */
        return;
    for (i = 0; i < pVBox->cScreens; ++i)
        VBoxVBVADisable(&pVBox->aVbvaCtx[i], &pVBox->guestCtx, i);
}
