/** @file $Id$
 *
 * VirtualBox X11 Additions graphics driver, DRI support
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
 * --------------------------------------------------------------------
 *
 * This code is based on:
 *
 * X11 TDFX driver, src/tdfx_dri.c
 *
 * Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *   Daryll Strauss <daryll@precisioninsight.com>
 */

#include "xf86.h"
#include "vboxvideo.h"
#ifndef PCIACCESS
# include "xf86Pci.h"
#endif
#include <dri.h>
#include <GL/glx.h>
#include <GL/glxint.h>
#include <drm.h>

static Bool
VBOXCreateContext(ScreenPtr pScreen, VisualPtr visual,
                  drm_context_t hwContext, void *pVisualConfigPriv,
                  DRIContextType contextStore);
static void
VBOXDestroyContext(ScreenPtr pScreen, drm_context_t hwContext,
                   DRIContextType contextStore);
static void
VBOXDRISwapContext(ScreenPtr pScreen, DRISyncType syncType,
                   DRIContextType oldContextType, void *oldContext,
                   DRIContextType newContextType, void *newContext);
static void
VBOXDRIInitBuffers(WindowPtr pWin, RegionPtr prgn, CARD32 index);
static void
VBOXDRIMoveBuffers(WindowPtr pParent, DDXPointRec ptOldOrg,
                   RegionPtr prgnSrc, CARD32 index);
static Bool
VBOXDRIOpenFullScreen(ScreenPtr pScreen);
static Bool
VBOXDRICloseFullScreen(ScreenPtr pScreen);
static void
VBOXDRITransitionTo2d(ScreenPtr pScreen);
static void
VBOXDRITransitionTo3d(ScreenPtr pScreen);

static Bool
VBOXInitVisualConfigs(ScrnInfoPtr pScrn, VBOXPtr pVBox)
{
    Bool rc = TRUE;
    TRACE_ENTRY();
    int cConfigs = 2;  /* With and without double buffering */
    __GLXvisualConfig *pConfigs = NULL;
    pConfigs = (__GLXvisualConfig*) calloc(sizeof(__GLXvisualConfig),
                                           cConfigs);
    if (!pConfigs)
    {
        rc = FALSE;
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "Disabling DRI: out of memory.\n");
    }
    for (int i = 0; rc && i < cConfigs; ++i)
    {
        pConfigs[i].vid = -1;
        pConfigs[i].class = -1;
        pConfigs[i].rgba = TRUE;
        if (pScrn->bitsPerPixel == 16)
        {
            pConfigs[i].redSize = 5;
            pConfigs[i].greenSize = 6;
            pConfigs[i].blueSize = 5;
            pConfigs[i].redMask = 0x0000F800;
            pConfigs[i].greenMask = 0x000007E0;
            pConfigs[i].blueMask = 0x0000001F;
        }
        else if (pScrn->bitsPerPixel == 32)
        {
            pConfigs[i].redSize = 8;
            pConfigs[i].greenSize = 8;
            pConfigs[i].blueSize = 8;
            pConfigs[i].alphaSize = 8;
            pConfigs[i].redMask   = 0x00ff0000;
            pConfigs[i].greenMask = 0x0000ff00;
            pConfigs[i].blueMask  = 0x000000ff;
            pConfigs[i].alphaMask = 0xff000000;
        }
        else
            rc = FALSE;
        pConfigs[i].bufferSize = pScrn->bitsPerPixel;
        pConfigs[i].visualRating = GLX_NONE;
        pConfigs[i].transparentPixel = GLX_NONE;
    }
    if (rc)
    {
        pConfigs[0].doubleBuffer = FALSE;
        pConfigs[1].doubleBuffer = TRUE;
        pVBox->cVisualConfigs = cConfigs;
        pVBox->pVisualConfigs = pConfigs;
        TRACE_LOG("Calling GlxSetVisualConfigs\n");
        GlxSetVisualConfigs(cConfigs, pConfigs, NULL);
    }
    if (!rc && pConfigs)
        free(pConfigs);
    TRACE_LOG("returning %s\n", BOOL_STR(rc));
    return rc;
}

#if 0
static void
VBOXDoWakeupHandler(int screenNum, pointer wakeupData, unsigned long result,
                    pointer pReadmask)
{

}
#endif

#if 0
static void
VBOXDoBlockHandler(int screenNum, pointer blockData, pointer pTimeout,
                   pointer pReadmask)
{

}
#endif

Bool VBOXDRIScreenInit(ScrnInfoPtr pScrn, ScreenPtr pScreen, VBOXPtr pVBox)
{
    DRIInfoPtr pDRIInfo = NULL;
    Bool rc = TRUE;

    TRACE_ENTRY();
    pVBox->drmFD = -1;
    if (   pScrn->bitsPerPixel != 16
        && pScrn->bitsPerPixel != 32)
    {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "DRI is only available in 16bpp or 32bpp graphics modes.\n");
        rc = FALSE;
    }
    /* Assertion */
    if (   (pScrn->displayWidth == 0)
        || (pVBox->pciInfo == NULL)
        || (pVBox->base == NULL)
        || (pVBox->cbFBMax == 0))
    {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "%s: preconditions failed\n",
                   __PRETTY_FUNCTION__);
        rc = FALSE;
    }
    /* Check that the GLX, DRI, and DRM modules have been loaded by testing for
     * canonical symbols in each module, the way all existing _dri drivers do.
     */
    if (rc)
    {
        TRACE_LOG("Checking symbols\n");
        if (   !xf86LoaderCheckSymbol("GlxSetVisualConfigs")
            || !xf86LoaderCheckSymbol("drmAvailable")
            || !xf86LoaderCheckSymbol("DRIQueryVersion"))
        {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                       "Disabling DRI due to missing server functionality.\n");
            rc = FALSE;
        }
    }
    /* Check the DRI version */
    if (rc)
    {
        int major, minor, patch;
        TRACE_LOG("Checking DRI version\n");
        DRIQueryVersion(&major, &minor, &patch);
        if (major != DRIINFO_MAJOR_VERSION || minor < DRIINFO_MINOR_VERSION)
        {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                       "Disabling DRI due to a version mismatch between server and driver.  Server version: %d.%d.  Driver version: %d.%d\n",
                       major, minor, DRIINFO_MAJOR_VERSION, DRIINFO_MINOR_VERSION);
            rc = FALSE;
        }
    }
    if (rc)
    {
        TRACE_LOG("Creating DRIInfoRec\n");
        pDRIInfo = DRICreateInfoRec();
        if (!pDRIInfo)
        {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                       "Disabling DRI: out of memory.\n");
            rc = FALSE;
        }
        else
            pVBox->pDRIInfo = pDRIInfo;
    }
    if (rc)
    {
        pDRIInfo->CreateContext = VBOXCreateContext;
        pDRIInfo->DestroyContext = VBOXDestroyContext;
        pDRIInfo->SwapContext = VBOXDRISwapContext;
        pDRIInfo->InitBuffers = VBOXDRIInitBuffers;
        pDRIInfo->MoveBuffers = VBOXDRIMoveBuffers;
        pDRIInfo->OpenFullScreen = VBOXDRIOpenFullScreen;
        pDRIInfo->CloseFullScreen = VBOXDRICloseFullScreen;
        pDRIInfo->TransitionTo2d = VBOXDRITransitionTo2d;
        pDRIInfo->TransitionTo3d = VBOXDRITransitionTo3d;

        /* These two are set in DRICreateInfoRec(). */
        pDRIInfo->wrap.ValidateTree = NULL;
        pDRIInfo->wrap.PostValidateTree = NULL;

        pDRIInfo->drmDriverName = VBOX_DRM_DRIVER_NAME;
        pDRIInfo->clientDriverName = VBOX_DRI_DRIVER_NAME;
#ifdef PCIACCESS
        pDRIInfo->busIdString = DRICreatePCIBusID(pVBox->pciInfo);
#else
        pDRIInfo->busIdString = alloc(64);
        sprintf(pDRIInfo->busIdString, "PCI:%d:%d:%d",
            ((pciConfigPtr)pVBox->pciInfo->thisCard)->busnum,
	        ((pciConfigPtr)pVBox->pciInfo->thisCard)->devnum,
            ((pciConfigPtr)pVBox->pciInfo->thisCard)->funcnum);
#endif
        pDRIInfo->ddxDriverMajorVersion = VBOX_VIDEO_MAJOR;
        pDRIInfo->ddxDriverMinorVersion = VBOX_VIDEO_MINOR;
        pDRIInfo->ddxDriverPatchVersion = 0;
        pDRIInfo->ddxDrawableTableEntry = VBOX_MAX_DRAWABLES;
        pDRIInfo->maxDrawableTableEntry = VBOX_MAX_DRAWABLES;
        pDRIInfo->frameBufferPhysicalAddress = (pointer)pScrn->memPhysBase;
        pDRIInfo->frameBufferSize = pVBox->cbFBMax;
        pDRIInfo->frameBufferStride =   pScrn->displayWidth
                                      * pScrn->bitsPerPixel / 8;
        pDRIInfo->SAREASize = SAREA_MAX;  /* we have no private bits yet. */
        /* This can't be zero, as the server callocs this size and checks for
         * non-NULL... */
        pDRIInfo->contextSize = 4;
        pDRIInfo->driverSwapMethod = DRI_HIDE_X_CONTEXT;
        pDRIInfo->bufferRequests = DRI_ALL_WINDOWS;
        TRACE_LOG("Calling DRIScreenInit\n");
        if (!DRIScreenInit(pScreen, pDRIInfo, &pVBox->drmFD))
            rc = FALSE;
        if (!rc)
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                       "DRIScreenInit failed, disabling DRI.\n");
    }
    if (rc && !VBOXInitVisualConfigs(pScrn, pVBox))
    {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "VBOXInitVisualConfigs failed, disabling DRI.\n");
        rc = FALSE;
    }
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "visual configurations initialized\n");

    /* Check the DRM version */
    if (rc)
    {
        drmVersionPtr version = drmGetVersion(pVBox->drmFD);
        TRACE_LOG("Checking DRM version\n");
        if (version)
        {
            if (version->version_major != 1 || version->version_minor < 0)
            {
                xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                           "Bad DRM driver version %d.%d, expected version 1.0.  Disabling DRI.\n",
                           version->version_major, version->version_minor);
                rc = FALSE;
            }
            drmFreeVersion(version);
        }
    }

    /* Clean up on failure. */
    if (!rc)
    {
        if (pVBox->pDRIInfo)
            DRIDestroyInfoRec(pVBox->pDRIInfo);
        pVBox->pDRIInfo = NULL;
        if (pVBox->drmFD >= 0)
           VBOXDRICloseScreen(pScreen, pVBox);
        pVBox->drmFD = -1;
    }
    TRACE_LOG("returning %s\n", BOOL_STR(rc));
    return rc;
}

void VBOXDRIUpdateStride(ScrnInfoPtr pScrn, VBOXPtr pVBox)
{
    DRIInfoPtr pDRIInfo = pVBox->pDRIInfo;
    pDRIInfo->frameBufferStride =   pScrn->displayWidth
                                  * pScrn->bitsPerPixel / 8;
}

void
VBOXDRICloseScreen(ScreenPtr pScreen, VBOXPtr pVBox)
{
    DRICloseScreen(pScreen);
    DRIDestroyInfoRec(pVBox->pDRIInfo);
    pVBox->pDRIInfo=0;
    if (pVBox->pVisualConfigs)
        free(pVBox->pVisualConfigs);
    pVBox->cVisualConfigs = 0;
    pVBox->pVisualConfigs = NULL;
}

static Bool
VBOXCreateContext(ScreenPtr pScreen, VisualPtr visual,
                  drm_context_t hwContext, void *pVisualConfigPriv,
                  DRIContextType contextStore)
{
    return TRUE;
}

static void
VBOXDestroyContext(ScreenPtr pScreen, drm_context_t hwContext,
                   DRIContextType contextStore)
{
}

Bool
VBOXDRIFinishScreenInit(ScreenPtr pScreen)
{
    return DRIFinishScreenInit(pScreen);
}

static void
VBOXDRISwapContext(ScreenPtr pScreen, DRISyncType syncType,
                   DRIContextType oldContextType, void *oldContext,
                   DRIContextType newContextType, void *newContext)
{
}

static void
VBOXDRIInitBuffers(WindowPtr pWin, RegionPtr prgn, CARD32 index)
{
}

static void
VBOXDRIMoveBuffers(WindowPtr pParent, DDXPointRec ptOldOrg,
                   RegionPtr prgnSrc, CARD32 index)
{
}

/* Apparently the next two are just legacy. */
static Bool
VBOXDRIOpenFullScreen(ScreenPtr pScreen)
{
    return TRUE;
}

static Bool
VBOXDRICloseFullScreen(ScreenPtr pScreen)
{
    return TRUE;
}

static void
VBOXDRITransitionTo2d(ScreenPtr pScreen)
{
}

static void
VBOXDRITransitionTo3d(ScreenPtr pScreen)
{
}

