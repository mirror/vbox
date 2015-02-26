/* $Id$ */
/** @file
 *
 * Linux Additions X11 graphics driver
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
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
 * This code is based on the X.Org VESA driver with the following copyrights:
 *
 * Copyright (c) 2000 by Conectiva S.A. (http://www.conectiva.com)
 * Copyright 2008 Red Hat, Inc.
 * Copyright 2012 Red Hat, Inc.
 *
 * and the following permission notice (not all original sourse files include
 * the last paragraph):
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * CONECTIVA LINUX BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of Conectiva Linux shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization from
 * Conectiva Linux.
 *
 * Authors: Paulo César Pereira de Andrade <pcpa@conectiva.com.br>
 *          David Dawes <dawes@xfree86.org>
 *          Adam Jackson <ajax@redhat.com>
 *          Dave Airlie <airlied@redhat.com>
 */

#ifdef XORG_7X
# include <stdlib.h>
# include <string.h>
#endif

#include "xf86.h"
#include "xf86_OSproc.h"
#if GET_ABI_MAJOR(ABI_VIDEODRV_VERSION) < 6
# include "xf86Resources.h"
#endif

/* This was accepted upstream in X.Org Server 1.16 which bumped the video
 * driver ABI to 17. */
#if GET_ABI_MAJOR(ABI_VIDEODRV_VERSION) < 17
# define SET_HAVE_VT_PROPERTY
#endif

#ifndef PCIACCESS
/* Drivers for PCI hardware need this */
# include "xf86PciInfo.h"
/* Drivers that need to access the PCI config space directly need this */
# include "xf86Pci.h"
#endif

#include "fb.h"
#include "os.h"

#include "vboxvideo.h"
#include <VBox/VBoxGuest.h>
#include <VBox/Hardware/VBoxVideoVBE.h>
#include "version-generated.h"
#include "product-generated.h"
#include <xf86.h>
#include <misc.h>

/* All drivers initialising the SW cursor need this */
#include "mipointer.h"

/* Colormap handling */
#include "micmap.h"
#include "xf86cmap.h"

/* DPMS */
/* #define DPMS_SERVER
#include "extensions/dpms.h" */

/* ShadowFB support */
#include "shadowfb.h"

/* VGA hardware functions for setting and restoring text mode */
#include "vgaHW.h"

#ifdef VBOXVIDEO_13
/* X.org 1.3+ mode setting */
# define _HAVE_STRING_ARCH_strsep /* bits/string2.h, __strsep_1c. */
# include "xf86Crtc.h"
# include "xf86Modes.h"
#endif

/* For setting the root window property. */
#include <X11/Xatom.h>
#include "property.h"

#ifdef VBOX_DRI
# include "xf86drm.h"
# include "xf86drmMode.h"
#endif

/* Mandatory functions */

static const OptionInfoRec * VBOXAvailableOptions(int chipid, int busid);
static void VBOXIdentify(int flags);
#ifndef PCIACCESS
static Bool VBOXProbe(DriverPtr drv, int flags);
#else
static Bool VBOXPciProbe(DriverPtr drv, int entity_num,
     struct pci_device *dev, intptr_t match_data);
#endif
static Bool VBOXPreInit(ScrnInfoPtr pScrn, int flags);
static Bool VBOXScreenInit(ScreenPtr pScreen, int argc, char **argv);
static Bool VBOXEnterVT(ScrnInfoPtr pScrn);
static void VBOXLeaveVT(ScrnInfoPtr pScrn);
static Bool VBOXCloseScreen(ScreenPtr pScreen);
static Bool VBOXSaveScreen(ScreenPtr pScreen, int mode);
static Bool VBOXSwitchMode(ScrnInfoPtr pScrn, DisplayModePtr pMode);
static void VBOXAdjustFrame(ScrnInfoPtr pScrn, int x, int y);
static void VBOXFreeScreen(ScrnInfoPtr pScrn);
static void VBOXDisplayPowerManagementSet(ScrnInfoPtr pScrn, int mode,
                                          int flags);

/* locally used functions */
static Bool VBOXMapVidMem(ScrnInfoPtr pScrn);
static void VBOXUnmapVidMem(ScrnInfoPtr pScrn);
static void VBOXSaveMode(ScrnInfoPtr pScrn);
static void VBOXRestoreMode(ScrnInfoPtr pScrn);
static void setSizesAndCursorIntegration(ScrnInfoPtr pScrn, bool fScreenInitTime);

#ifndef XF86_SCRN_INTERFACE
# define xf86ScreenToScrn(pScreen) xf86Screens[(pScreen)->myNum]
# define xf86ScrnToScreen(pScrn) screenInfo.screens[(pScrn)->scrnIndex]
#endif

static inline void VBOXSetRec(ScrnInfoPtr pScrn)
{
    if (!pScrn->driverPrivate)
    {
        VBOXPtr pVBox = (VBOXPtr)xnfcalloc(sizeof(VBOXRec), 1);
        pScrn->driverPrivate = pVBox;
    }
}

enum GenericTypes
{
    CHIP_VBOX_GENERIC
};

#ifdef PCIACCESS
static const struct pci_id_match vbox_device_match[] = {
    {
        VBOX_VENDORID, VBOX_DEVICEID, PCI_MATCH_ANY, PCI_MATCH_ANY,
        0, 0, 0
    },

    { 0, 0, 0 },
};
#endif

/* Supported chipsets */
static SymTabRec VBOXChipsets[] =
{
    {VBOX_DEVICEID, "vbox"},
    {-1,            NULL}
};

static PciChipsets VBOXPCIchipsets[] = {
  { VBOX_DEVICEID, VBOX_DEVICEID, RES_SHARED_VGA },
  { -1,            -1,            RES_UNDEFINED },
};

/*
 * This contains the functions needed by the server after loading the
 * driver module.  It must be supplied, and gets added the driver list by
 * the Module Setup function in the dynamic case.  In the static case a
 * reference to this is compiled in, and this requires that the name of
 * this DriverRec be an upper-case version of the driver name.
 */

#ifdef XORG_7X
_X_EXPORT
#endif
DriverRec VBOXVIDEO = {
    VBOX_VERSION,
    VBOX_DRIVER_NAME,
    VBOXIdentify,
#ifdef PCIACCESS
    NULL,
#else
    VBOXProbe,
#endif
    VBOXAvailableOptions,
    NULL,
    0,
#ifdef XORG_7X
    NULL,
#endif
#ifdef PCIACCESS
    vbox_device_match,
    VBOXPciProbe
#endif
};

/* No options for now */
static const OptionInfoRec VBOXOptions[] = {
    { -1, NULL, OPTV_NONE, {0}, FALSE }
};

#ifndef XORG_7X
/*
 * List of symbols from other modules that this module references.  This
 * list is used to tell the loader that it is OK for symbols here to be
 * unresolved providing that it hasn't been told that they haven't been
 * told that they are essential via a call to xf86LoaderReqSymbols() or
 * xf86LoaderReqSymLists().  The purpose is this is to avoid warnings about
 * unresolved symbols that are not required.
 */
static const char *fbSymbols[] = {
    "fbPictureInit",
    "fbScreenInit",
    NULL
};

static const char *shadowfbSymbols[] = {
    "ShadowFBInit2",
    NULL
};

static const char *ramdacSymbols[] = {
    "xf86DestroyCursorInfoRec",
    "xf86InitCursor",
    "xf86CreateCursorInfoRec",
    NULL
};

static const char *vgahwSymbols[] = {
    "vgaHWFreeHWRec",
    "vgaHWGetHWRec",
    "vgaHWGetIOBase",
    "vgaHWGetIndex",
    "vgaHWRestore",
    "vgaHWSave",
    "vgaHWSetStdFuncs",
    NULL
};
#endif /* !XORG_7X */

/** Resize the virtual framebuffer. */
static Bool adjustScreenPixmap(ScrnInfoPtr pScrn, int width, int height)
{
    ScreenPtr pScreen = xf86ScrnToScreen(pScrn);
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    int adjustedWidth = pScrn->bitsPerPixel == 16 ? (width + 1) & ~1 : width;
    int cbLine = adjustedWidth * pScrn->bitsPerPixel / 8;
    PixmapPtr pPixmap;
    int rc;

    TRACE_LOG("width=%d, height=%d\n", width, height);
    VBVXASSERT(width >= 0 && height >= 0, ("Invalid negative width (%d) or height (%d)\n", width, height));
    if (pScreen == NULL)  /* Not yet initialised. */
        return TRUE;
    pPixmap = pScreen->GetScreenPixmap(pScreen);
    VBVXASSERT(pPixmap != NULL, ("Failed to get the screen pixmap.\n"));
    if (   adjustedWidth != pPixmap->drawable.width
        || height != pPixmap->drawable.height)
    {
        if (adjustedWidth > INT16_MAX || height > INT16_MAX || (unsigned)cbLine * (unsigned)height >= pVBox->cbFBMax)
        {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                       "Virtual framebuffer %dx%d too large (%u Kb available).\n",
                       adjustedWidth, height, (unsigned) pVBox->cbFBMax / 1024);
            return FALSE;
        }
        vbvxClearVRAM(pScrn, pScrn->virtualX * pScrn->virtualY * pScrn->bitsPerPixel,
                      adjustedWidth * height * pScrn->bitsPerPixel);
        pScreen->ModifyPixmapHeader(pPixmap, adjustedWidth, height, pScrn->depth, pScrn->bitsPerPixel, cbLine, pVBox->base);
    }
    pScrn->virtualX = adjustedWidth;
    pScrn->virtualY = height;
    pScrn->displayWidth = adjustedWidth;
    pVBox->cbLine = cbLine;
#ifdef VBOX_DRI_OLD
    if (pVBox->useDRI)
        VBOXDRIUpdateStride(pScrn, pVBox);
#endif
    return TRUE;
}

/** Set a video mode to the hardware, RandR 1.1 version.  Since we no longer do
 * virtual frame buffers, adjust the screen pixmap dimensions to match. */
static void setModeRandR11(ScrnInfoPtr pScrn, DisplayModePtr pMode)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    struct vbvxFrameBuffer frameBuffer = { 0, 0, pMode->HDisplay, pMode->VDisplay, pScrn->bitsPerPixel};

    adjustScreenPixmap(pScrn, pMode->HDisplay, pMode->VDisplay);
    if (pMode->HDisplay != 0 && pMode->VDisplay != 0)
        vbvxSetMode(pScrn, 0, pMode->HDisplay, pMode->VDisplay, 0, 0, true, true, &frameBuffer);
}

#ifdef VBOXVIDEO_13
/* X.org 1.3+ mode-setting support ******************************************/

/** Set a video mode to the hardware, RandR 1.2 version.  If this is the first
 * screen, re-set the current mode for all others (the offset for the first
 * screen is always treated as zero by the hardware, so all other screens need
 * to be changed to compensate for any changes!).  The mode to set is taken
 * from the X.Org Crtc structure. */
static void setModeRandR12(ScrnInfoPtr pScrn, unsigned cScreen)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    unsigned i;
    struct vbvxFrameBuffer frameBuffer = { pVBox->pScreens[0].paCrtcs->x, pVBox->pScreens[0].paCrtcs->y, pScrn->virtualX,
                                           pScrn->virtualY, pScrn->bitsPerPixel };
    unsigned cFirst = cScreen;
    unsigned cLast = cScreen != 0 ? cScreen + 1 : pVBox->cScreens;

    for (i = cFirst; i < cLast; ++i)
        if (pVBox->pScreens[i].paCrtcs->mode.HDisplay != 0 && pVBox->pScreens[i].paCrtcs->mode.VDisplay != 0)
            vbvxSetMode(pScrn, i, pVBox->pScreens[i].paCrtcs->mode.HDisplay, pVBox->pScreens[i].paCrtcs->mode.VDisplay,
                        pVBox->pScreens[i].paCrtcs->x, pVBox->pScreens[i].paCrtcs->y, pVBox->pScreens[i].fPowerOn,
                        pVBox->pScreens[i].paOutputs->status == XF86OutputStatusConnected, &frameBuffer);
}

/** Wrapper around setModeRandR12() to avoid exposing non-obvious semantics.
 */
static void setAllModesRandR12(ScrnInfoPtr pScrn)
{
    setModeRandR12(pScrn, 0);
}

/* For descriptions of these functions and structures, see
   hw/xfree86/modes/xf86Crtc.h and hw/xfree86/modes/xf86Modes.h in the
   X.Org source tree. */

static Bool vbox_config_resize(ScrnInfoPtr pScrn, int cw, int ch)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    Bool rc;
    unsigned i;

    TRACE_LOG("width=%d, height=%d\n", cw, ch);
    /* Don't fiddle with the hardware if we are switched
     * to a virtual terminal. */
    if (!pScrn->vtSema) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "We do not own the active VT, exiting.\n");
        return TRUE;
    }
    rc = adjustScreenPixmap(pScrn, cw, ch);
    /* Power-on all screens (the server expects this) and set the new pitch to them. */
    for (i = 0; i < pVBox->cScreens; ++i)
        pVBox->pScreens[i].fPowerOn = true;
    setAllModesRandR12(pScrn);
    vbvxSetSolarisMouseRange(cw, ch);
    return rc;
}

static const xf86CrtcConfigFuncsRec VBOXCrtcConfigFuncs = {
    vbox_config_resize
};

static void
vbox_crtc_dpms(xf86CrtcPtr crtc, int mode)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    unsigned cDisplay = (uintptr_t)crtc->driver_private;

    TRACE_LOG("mode=%d\n", mode);
    pVBox->pScreens[cDisplay].fPowerOn = (mode != DPMSModeOff);
    setModeRandR12(pScrn, cDisplay);
}

static Bool
vbox_crtc_lock (xf86CrtcPtr crtc)
{ (void) crtc; return FALSE; }


/* We use this function to check whether the X server owns the active virtual
 * terminal before attempting a mode switch, since the RandR extension isn't
 * very dilligent here, which can mean crashes if we are unlucky.  This is
 * not the way it the function is intended - it is meant for reporting modes
 * which the hardware can't handle.  I hope that this won't confuse any clients
 * connecting to us. */
static Bool
vbox_crtc_mode_fixup (xf86CrtcPtr crtc, DisplayModePtr mode,
                      DisplayModePtr adjusted_mode)
{ (void) crtc; (void) mode; (void) adjusted_mode; return TRUE; }

static void
vbox_crtc_stub (xf86CrtcPtr crtc)
{ (void) crtc; }

static void
vbox_crtc_mode_set (xf86CrtcPtr crtc, DisplayModePtr mode,
                    DisplayModePtr adjusted_mode, int x, int y)
{
    (void) mode;
    VBOXPtr pVBox = VBOXGetRec(crtc->scrn);
    unsigned cDisplay = (uintptr_t)crtc->driver_private;

    TRACE_LOG("name=%s, HDisplay=%d, VDisplay=%d, x=%d, y=%d\n", adjusted_mode->name,
           adjusted_mode->HDisplay, adjusted_mode->VDisplay, x, y);
    pVBox->pScreens[cDisplay].fPowerOn = true;
    pVBox->pScreens[cDisplay].aScreenLocation.cx = adjusted_mode->HDisplay;
    pVBox->pScreens[cDisplay].aScreenLocation.cy = adjusted_mode->VDisplay;
    pVBox->pScreens[cDisplay].aScreenLocation.x = x;
    pVBox->pScreens[cDisplay].aScreenLocation.y = y;
    /* Don't fiddle with the hardware if we are switched
     * to a virtual terminal. */
    if (!crtc->scrn->vtSema)
    {
        xf86DrvMsg(crtc->scrn->scrnIndex, X_ERROR,
                   "We do not own the active VT, exiting.\n");
        return;
    }
    setModeRandR12(crtc->scrn, cDisplay);
}

static void
vbox_crtc_gamma_set (xf86CrtcPtr crtc, CARD16 *red,
                     CARD16 *green, CARD16 *blue, int size)
{ (void) crtc; (void) red; (void) green; (void) blue; (void) size; }

static void *
vbox_crtc_shadow_allocate (xf86CrtcPtr crtc, int width, int height)
{ (void) crtc; (void) width; (void) height; return NULL; }

static const xf86CrtcFuncsRec VBOXCrtcFuncs = {
    .dpms = vbox_crtc_dpms,
    .save = NULL, /* These two are never called by the server. */
    .restore = NULL,
    .lock = vbox_crtc_lock,
    .unlock = NULL, /* This will not be invoked if lock returns FALSE. */
    .mode_fixup = vbox_crtc_mode_fixup,
    .prepare = vbox_crtc_stub,
    .mode_set = vbox_crtc_mode_set,
    .commit = vbox_crtc_stub,
    .gamma_set = vbox_crtc_gamma_set,
    .shadow_allocate = vbox_crtc_shadow_allocate,
    .shadow_create = NULL, /* These two should not be invoked if allocate
                              returns NULL. */
    .shadow_destroy = NULL,
    .set_cursor_colors = NULL, /* We are still using the old cursor API. */
    .set_cursor_position = NULL,
    .show_cursor = NULL,
    .hide_cursor = NULL,
    .load_cursor_argb = NULL,
    .destroy = vbox_crtc_stub
};

static void
vbox_output_stub (xf86OutputPtr output)
{ (void) output; }

static void
vbox_output_dpms (xf86OutputPtr output, int mode)
{
    (void)output; (void)mode;
}

static int
vbox_output_mode_valid (xf86OutputPtr output, DisplayModePtr mode)
{
    return MODE_OK;
}

static Bool
vbox_output_mode_fixup (xf86OutputPtr output, DisplayModePtr mode,
                        DisplayModePtr adjusted_mode)
{ (void) output; (void) mode; (void) adjusted_mode; return TRUE; }

static void
vbox_output_mode_set (xf86OutputPtr output, DisplayModePtr mode,
                        DisplayModePtr adjusted_mode)
{ (void) output; (void) mode; (void) adjusted_mode; }

/* A virtual monitor is always connected. */
static xf86OutputStatus
vbox_output_detect (xf86OutputPtr output)
{
    ScrnInfoPtr pScrn = output->scrn;
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    uint32_t iScreen = (uintptr_t)output->driver_private;
    return   pVBox->pScreens[iScreen].afConnected
           ? XF86OutputStatusConnected : XF86OutputStatusDisconnected;
}

static DisplayModePtr vbox_output_add_mode(VBOXPtr pVBox, DisplayModePtr *pModes, const char *pszName, int x, int y,
                                           Bool isPreferred, Bool isUserDef)
{
    TRACE_LOG("pszName=%s, x=%d, y=%d\n", pszName ? pszName : "(null)", x, y);
    DisplayModePtr pMode = xnfcalloc(1, sizeof(DisplayModeRec));
    int cRefresh = 60;

    pMode->status        = MODE_OK;
    /* We don't ask the host whether it likes user defined modes,
     * as we assume that the user really wanted that mode. */
    pMode->type          = isUserDef ? M_T_USERDEF : M_T_BUILTIN;
    if (isPreferred)
        pMode->type     |= M_T_PREFERRED;
    /* Older versions of VBox only support screen widths which are a multiple
     * of 8 */
    if (pVBox->fAnyX)
        pMode->HDisplay  = x;
    else
        pMode->HDisplay  = x & ~7;
    pMode->HSyncStart    = pMode->HDisplay + 2;
    pMode->HSyncEnd      = pMode->HDisplay + 4;
    pMode->HTotal        = pMode->HDisplay + 6;
    pMode->VDisplay      = y;
    pMode->VSyncStart    = pMode->VDisplay + 2;
    pMode->VSyncEnd      = pMode->VDisplay + 4;
    pMode->VTotal        = pMode->VDisplay + 6;
    pMode->Clock         = pMode->HTotal * pMode->VTotal * cRefresh / 1000; /* kHz */
    if (NULL == pszName) {
        xf86SetModeDefaultName(pMode);
    } else {
        pMode->name          = xnfstrdup(pszName);
    }
    *pModes = xf86ModesAdd(*pModes, pMode);
    return pMode;
}

static DisplayModePtr
vbox_output_get_modes (xf86OutputPtr output)
{
    unsigned i, cIndex = 0;
    DisplayModePtr pModes = NULL, pMode;
    ScrnInfoPtr pScrn = output->scrn;
    VBOXPtr pVBox = VBOXGetRec(pScrn);

    TRACE_ENTRY();
    uint32_t x, y, iScreen;
    iScreen = (uintptr_t)output->driver_private;
    pMode = vbox_output_add_mode(pVBox, &pModes, NULL, pVBox->pScreens[iScreen].aPreferredSize.cx,
                                 pVBox->pScreens[iScreen].aPreferredSize.cy, TRUE, FALSE);
    VBOXEDIDSet(output, pMode);
    TRACE_EXIT();
    return pModes;
}

static const xf86OutputFuncsRec VBOXOutputFuncs = {
    .create_resources = vbox_output_stub,
    .dpms = vbox_output_dpms,
    .save = NULL, /* These two are never called by the server. */
    .restore = NULL,
    .mode_valid = vbox_output_mode_valid,
    .mode_fixup = vbox_output_mode_fixup,
    .prepare = vbox_output_stub,
    .commit = vbox_output_stub,
    .mode_set = vbox_output_mode_set,
    .detect = vbox_output_detect,
    .get_modes = vbox_output_get_modes,
#ifdef RANDR_12_INTERFACE
     .set_property = NULL,
#endif
    .destroy = vbox_output_stub
};
#endif /* VBOXVIDEO_13 */

/* Module loader interface */
static MODULESETUPPROTO(vboxSetup);

static XF86ModuleVersionInfo vboxVersionRec =
{
    VBOX_DRIVER_NAME,
    VBOX_VENDOR,
    MODINFOSTRING1,
    MODINFOSTRING2,
#ifdef XORG_7X
    XORG_VERSION_CURRENT,
#else
    XF86_VERSION_CURRENT,
#endif
    1,                          /* Module major version. Xorg-specific */
    0,                          /* Module minor version. Xorg-specific */
    1,                          /* Module patchlevel. Xorg-specific */
    ABI_CLASS_VIDEODRV,         /* This is a video driver */
    ABI_VIDEODRV_VERSION,
    MOD_CLASS_VIDEODRV,
    {0, 0, 0, 0}
};

/*
 * This data is accessed by the loader.  The name must be the module name
 * followed by "ModuleData".
 */
#ifdef XORG_7X
_X_EXPORT
#endif
XF86ModuleData vboxvideoModuleData = { &vboxVersionRec, vboxSetup, NULL };

static pointer
vboxSetup(pointer Module, pointer Options, int *ErrorMajor, int *ErrorMinor)
{
    static Bool Initialised = FALSE;

    if (!Initialised)
    {
        Initialised = TRUE;
#ifdef PCIACCESS
        xf86AddDriver(&VBOXVIDEO, Module, HaveDriverFuncs);
#else
        xf86AddDriver(&VBOXVIDEO, Module, 0);
#endif
#ifndef XORG_7X
        LoaderRefSymLists(fbSymbols,
                          shadowfbSymbols,
                          ramdacSymbols,
                          vgahwSymbols,
                          NULL);
#endif
        xf86Msg(X_CONFIG, "Load address of symbol \"VBOXVIDEO\" is %p\n",
                (void *)&VBOXVIDEO);
        return (pointer)TRUE;
    }

    if (ErrorMajor)
        *ErrorMajor = LDR_ONCEONLY;
    return (NULL);
}


static const OptionInfoRec *
VBOXAvailableOptions(int chipid, int busid)
{
    return (VBOXOptions);
}

static void
VBOXIdentify(int flags)
{
    xf86PrintChipsets(VBOX_NAME, "guest driver for VirtualBox", VBOXChipsets);
}

#ifndef XF86_SCRN_INTERFACE
# define SCRNINDEXAPI(pfn) pfn ## Index
static Bool VBOXScreenInitIndex(int scrnIndex, ScreenPtr pScreen, int argc,
                                char **argv)
{ return VBOXScreenInit(pScreen, argc, argv); }

static Bool VBOXEnterVTIndex(int scrnIndex, int flags)
{ (void) flags; return VBOXEnterVT(xf86Screens[scrnIndex]); }

static void VBOXLeaveVTIndex(int scrnIndex, int flags)
{ (void) flags; VBOXLeaveVT(xf86Screens[scrnIndex]); }

static Bool VBOXCloseScreenIndex(int scrnIndex, ScreenPtr pScreen)
{ (void) scrnIndex; return VBOXCloseScreen(pScreen); }

static Bool VBOXSwitchModeIndex(int scrnIndex, DisplayModePtr pMode, int flags)
{ (void) flags; return VBOXSwitchMode(xf86Screens[scrnIndex], pMode); }

static void VBOXAdjustFrameIndex(int scrnIndex, int x, int y, int flags)
{ (void) flags; VBOXAdjustFrame(xf86Screens[scrnIndex], x, y); }

static void VBOXFreeScreenIndex(int scrnIndex, int flags)
{ (void) flags; VBOXFreeScreen(xf86Screens[scrnIndex]); }
# else
# define SCRNINDEXAPI(pfn) pfn
#endif /* XF86_SCRN_INTERFACE */

static void setScreenFunctions(ScrnInfoPtr pScrn, xf86ProbeProc pfnProbe)
{
    pScrn->driverVersion = VBOX_VERSION;
    pScrn->driverName    = VBOX_DRIVER_NAME;
    pScrn->name          = VBOX_NAME;
    pScrn->Probe         = pfnProbe;
    pScrn->PreInit       = VBOXPreInit;
    pScrn->ScreenInit    = SCRNINDEXAPI(VBOXScreenInit);
    pScrn->SwitchMode    = SCRNINDEXAPI(VBOXSwitchMode);
    pScrn->AdjustFrame   = SCRNINDEXAPI(VBOXAdjustFrame);
    pScrn->EnterVT       = SCRNINDEXAPI(VBOXEnterVT);
    pScrn->LeaveVT       = SCRNINDEXAPI(VBOXLeaveVT);
    pScrn->FreeScreen    = SCRNINDEXAPI(VBOXFreeScreen);
}

/*
 * One of these functions is called once, at the start of the first server
 * generation to do a minimal probe for supported hardware.
 */

#ifdef PCIACCESS
static Bool
VBOXPciProbe(DriverPtr drv, int entity_num, struct pci_device *dev,
             intptr_t match_data)
{
    ScrnInfoPtr pScrn;

    TRACE_ENTRY();
    pScrn = xf86ConfigPciEntity(NULL, 0, entity_num, VBOXPCIchipsets,
                                NULL, NULL, NULL, NULL, NULL);
    if (pScrn != NULL) {
        VBOXPtr pVBox;

        VBOXSetRec(pScrn);
        pVBox = VBOXGetRec(pScrn);
        if (!pVBox)
            return FALSE;
        setScreenFunctions(pScrn, NULL);
        pVBox->pciInfo = dev;
    }

    TRACE_LOG("returning %s\n", BOOL_STR(pScrn != NULL));
    return (pScrn != NULL);
}
#endif

#ifndef PCIACCESS
static Bool
VBOXProbe(DriverPtr drv, int flags)
{
    Bool foundScreen = FALSE;
    int numDevSections;
    GDevPtr *devSections;

    /*
     * Find the config file Device sections that match this
     * driver, and return if there are none.
     */
    if ((numDevSections = xf86MatchDevice(VBOX_NAME,
                      &devSections)) <= 0)
    return (FALSE);

    /* PCI BUS */
    if (xf86GetPciVideoInfo())
    {
        int numUsed;
        int *usedChips;
        int i;
        numUsed = xf86MatchPciInstances(VBOX_NAME, VBOX_VENDORID,
                        VBOXChipsets, VBOXPCIchipsets,
                        devSections, numDevSections,
                        drv, &usedChips);
        if (numUsed > 0)
        {
            if (flags & PROBE_DETECT)
                foundScreen = TRUE;
            else
                for (i = 0; i < numUsed; i++)
                {
                    ScrnInfoPtr pScrn = NULL;
                    /* Allocate a ScrnInfoRec  */
                    if ((pScrn = xf86ConfigPciEntity(pScrn,0,usedChips[i],
                                     VBOXPCIchipsets,NULL,
                                     NULL,NULL,NULL,NULL)))
                    {
                        setScreenFunctions(pScrn, VBOXProbe);
                        foundScreen = TRUE;
                    }
                }
            free(usedChips);
        }
    }
    free(devSections);
    return (foundScreen);
}
#endif


/*
 * QUOTE from the XFree86 DESIGN document:
 *
 * The purpose of this function is to find out all the information
 * required to determine if the configuration is usable, and to initialise
 * those parts of the ScrnInfoRec that can be set once at the beginning of
 * the first server generation.
 *
 * (...)
 *
 * This includes probing for video memory, clocks, ramdac, and all other
 * HW info that is needed. It includes determining the depth/bpp/visual
 * and related info. It includes validating and determining the set of
 * video modes that will be used (and anything that is required to
 * determine that).
 *
 * This information should be determined in the least intrusive way
 * possible. The state of the HW must remain unchanged by this function.
 * Although video memory (including MMIO) may be mapped within this
 * function, it must be unmapped before returning.
 *
 * END QUOTE
 */

static Bool
VBOXPreInit(ScrnInfoPtr pScrn, int flags)
{
    VBOXPtr pVBox;
    Gamma gzeros = {0.0, 0.0, 0.0};
    rgb rzeros = {0, 0, 0};
    unsigned DispiId;

    TRACE_ENTRY();
    /* Are we really starting the server, or is this just a dummy run? */
    if (flags & PROBE_DETECT)
        return (FALSE);

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
               "VirtualBox guest additions video driver version "
               VBOX_VERSION_STRING "\n");

    /* Get our private data from the ScrnInfoRec structure. */
    VBOXSetRec(pScrn);
    pVBox = VBOXGetRec(pScrn);
    if (!pVBox)
        return FALSE;

    /* Entity information seems to mean bus information. */
    pVBox->pEnt = xf86GetEntityInfo(pScrn->entityList[0]);

    /* The ramdac module is needed for the hardware cursor. */
    if (!xf86LoadSubModule(pScrn, "ramdac"))
        return FALSE;

    /* The framebuffer module. */
    if (!xf86LoadSubModule(pScrn, "fb"))
        return (FALSE);

    if (!xf86LoadSubModule(pScrn, "shadowfb"))
        return FALSE;

    if (!xf86LoadSubModule(pScrn, "vgahw"))
        return FALSE;

#ifdef VBOX_DRI_OLD
    /* Load the dri module. */
    if (!xf86LoadSubModule(pScrn, "dri"))
        return FALSE;
#else
# ifdef VBOX_DRI
    /* Load the dri module. */
    if (!xf86LoadSubModule(pScrn, "dri2"))
        return FALSE;
# endif
#endif

#ifndef PCIACCESS
    if (pVBox->pEnt->location.type != BUS_PCI)
        return FALSE;

    pVBox->pciInfo = xf86GetPciInfoForEntity(pVBox->pEnt->index);
    pVBox->pciTag = pciTag(pVBox->pciInfo->bus,
                           pVBox->pciInfo->device,
                           pVBox->pciInfo->func);
#endif

    /* Set up our ScrnInfoRec structure to describe our virtual
       capabilities to X. */

    pScrn->chipset = "vbox";
    /** @note needed during colourmap initialisation */
    pScrn->rgbBits = 8;

    /* Let's create a nice, capable virtual monitor. */
    pScrn->monitor = pScrn->confScreen->monitor;
    pScrn->monitor->DDC = NULL;
    pScrn->monitor->nHsync = 1;
    pScrn->monitor->hsync[0].lo = 1;
    pScrn->monitor->hsync[0].hi = 10000;
    pScrn->monitor->nVrefresh = 1;
    pScrn->monitor->vrefresh[0].lo = 1;
    pScrn->monitor->vrefresh[0].hi = 100;

    pScrn->progClock = TRUE;

    /* Using the PCI information caused problems with non-powers-of-two
       sized video RAM configurations */
    pVBox->cbFBMax = VBoxVideoGetVRAMSize();
    pScrn->videoRam = pVBox->cbFBMax / 1024;

    /* Check if the chip restricts horizontal resolution or not. */
    pVBox->fAnyX = VBoxVideoAnyWidthAllowed();

    /* Set up clock information that will support all modes we need. */
    pScrn->clockRanges = xnfcalloc(sizeof(ClockRange), 1);
    pScrn->clockRanges->minClock = 1000;
    pScrn->clockRanges->maxClock = 1000000000;
    pScrn->clockRanges->clockIndex = -1;
    pScrn->clockRanges->ClockMulFactor = 1;
    pScrn->clockRanges->ClockDivFactor = 1;

    if (!xf86SetDepthBpp(pScrn, 24, 0, 0, Support32bppFb))
        return FALSE;
    /* We only support 16 and 24 bits depth (i.e. 16 and 32bpp) */
    if (pScrn->bitsPerPixel != 32 && pScrn->bitsPerPixel != 16)
    {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "The VBox additions only support 16 and 32bpp graphics modes\n");
        return FALSE;
    }
    xf86PrintDepthBpp(pScrn);
    vboxAddModes(pScrn);

#ifdef VBOXVIDEO_13
    /* Work around a bug in the original X server modesetting code, which
     * took the first valid values set to these two as maxima over the
     * server lifetime. */
    pScrn->virtualX = 32000;
    pScrn->virtualY = 32000;
#else
    /* We don't validate with xf86ValidateModes and xf86PruneModes as we
     * already know what we like and what we don't. */

    pScrn->currentMode = pScrn->modes;

    /* Set the right virtual resolution. */
    pScrn->virtualX = pScrn->bitsPerPixel == 16 ? (pScrn->currentMode->HDisplay + 1) & ~1 : pScrn->currentMode->HDisplay;
    pScrn->virtualY = pScrn->currentMode->VDisplay;

#endif /* !VBOXVIDEO_13 */

    /* Needed before we initialise DRI. */
    pScrn->displayWidth = pScrn->virtualX;
    pVBox->cbLine = pScrn->virtualX * pScrn->bitsPerPixel / 8;

    xf86PrintModes(pScrn);

    /* VGA hardware initialisation */
    if (!vgaHWGetHWRec(pScrn))
        return FALSE;
    /* Must be called before any VGA registers are saved or restored */
    vgaHWSetStdFuncs(VGAHWPTR(pScrn));
    vgaHWGetIOBase(VGAHWPTR(pScrn));

    /* Colour weight - we always call this, since we are always in
       truecolour. */
    if (!xf86SetWeight(pScrn, rzeros, rzeros))
        return (FALSE);

    /* visual init */
    if (!xf86SetDefaultVisual(pScrn, -1))
        return (FALSE);

    xf86SetGamma(pScrn, gzeros);

    /* Set the DPI.  Perhaps we should read this from the host? */
    xf86SetDpi(pScrn, 96, 96);

    if (pScrn->memPhysBase == 0) {
#ifdef PCIACCESS
        pScrn->memPhysBase = pVBox->pciInfo->regions[0].base_addr;
#else
        pScrn->memPhysBase = pVBox->pciInfo->memBase[0];
#endif
        pScrn->fbOffset = 0;
    }

    TRACE_EXIT();
    return (TRUE);
}

/**
 * Dummy function for setting the colour palette, which we actually never
 * touch.  However, the server still requires us to provide this.
 */
static void
vboxLoadPalette(ScrnInfoPtr pScrn, int numColors, int *indices,
          LOCO *colors, VisualPtr pVisual)
{
    (void)pScrn; (void) numColors; (void) indices; (void) colors;
    (void)pVisual;
}

#define HAS_VT_ATOM_NAME "XFree86_has_VT"
#define VBOXVIDEO_DRIVER_ATOM_NAME "VBOXVIDEO_DRIVER_IN_USE"
/* The memory storing the initial value of the XFree86_has_VT root window
 * property.  This has to remain available until server start-up, so we just
 * use a global. */
static CARD32 InitialPropertyValue = 1;

/** Initialise a flag property on the root window to say whether the server VT
 *  is currently the active one as some clients need to know this. */
static void initialiseProperties(ScrnInfoPtr pScrn)
{
    Atom atom = -1;
    CARD32 *PropertyValue = &InitialPropertyValue;
#ifdef SET_HAVE_VT_PROPERTY
    atom = MakeAtom(HAS_VT_ATOM_NAME, sizeof(HAS_VT_ATOM_NAME) - 1, TRUE);
    if (xf86RegisterRootWindowProperty(pScrn->scrnIndex, atom, XA_INTEGER,
                                       32, 1, PropertyValue) != Success)
        FatalError("vboxvideo: failed to register VT property\n");
#endif /* SET_HAVE_VT_PROPERTY */
    atom = MakeAtom(VBOXVIDEO_DRIVER_ATOM_NAME,
                    sizeof(VBOXVIDEO_DRIVER_ATOM_NAME) - 1, TRUE);
    if (xf86RegisterRootWindowProperty(pScrn->scrnIndex, atom, XA_INTEGER,
                                       32, 1, PropertyValue) != Success)
        FatalError("vboxvideo: failed to register driver in use property\n");
}

#ifdef SET_HAVE_VT_PROPERTY
/** Update a flag property on the root window to say whether the server VT
 *  is currently the active one as some clients need to know this. */
static void updateHasVTProperty(ScrnInfoPtr pScrn, Bool hasVT)
{
    Atom property_name;
    int32_t value = hasVT ? 1 : 0;
    int i;

    property_name = MakeAtom(HAS_VT_ATOM_NAME, sizeof(HAS_VT_ATOM_NAME) - 1,
                             FALSE);
    if (property_name == BAD_RESOURCE)
        FatalError("Failed to retrieve \"HAS_VT\" atom\n");
    if (ROOT_WINDOW(pScrn) == NULL)
        return;
    ChangeWindowProperty(ROOT_WINDOW(pScrn), property_name, XA_INTEGER, 32,
                         PropModeReplace, 1, &value, TRUE);
}
#endif /* SET_HAVE_VT_PROPERTY */

#ifdef VBOXVIDEO_13
static void getVirtualSize(ScrnInfoPtr pScrn, int *px, int *py)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    unsigned i;

    *px = 0;
    *py = 0;
    for (i = 0; i < pVBox->cScreens; ++i)
    {
        if (pVBox->pScreens[i].paOutputs->status == XF86OutputStatusConnected)
        {
            *px += pVBox->pScreens[i].aPreferredSize.cx;
            *py = RT_MAX(*py, VBVXCAST(int, pVBox->pScreens[i].aPreferredSize.cy));
        }
    }
}
#endif

static void setSizesAndCursorIntegration(ScrnInfoPtr pScrn, bool fScreenInitTime)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);
#ifdef VBOXVIDEO_13
    unsigned i;
    int x, y;
#else
    DisplayModePtr pNewMode;
#endif

    TRACE_LOG("fScreenInitTime=%d\n", fScreenInitTime);
#ifdef VBOXVIDEO_13
# if GET_ABI_MAJOR(ABI_VIDEODRV_VERSION) >= 5
    RRGetInfo(xf86ScrnToScreen(pScrn), TRUE);
# else
    RRGetInfo(xf86ScrnToScreen(pScrn));
# endif
    getVirtualSize(pScrn, &x, &y);
    if (fScreenInitTime)
    {
        pScrn->virtualX = x;
        pScrn->virtualY = y;
    }
    else
        RRScreenSizeSet(xf86ScrnToScreen(pScrn), x, y, xf86ScrnToScreen(pScrn)->mmWidth, xf86ScrnToScreen(pScrn)->mmHeight);
    x = 0;
    for (i = 0; i < pVBox->cScreens; ++i)
    {
        if (fScreenInitTime)
            xf86CrtcSetMode(pVBox->pScreens[i].paCrtcs, pVBox->pScreens[i].paOutputs->probed_modes, RR_Rotate_0, x, 0);
        else
            RRCrtcSet(pVBox->pScreens[i].paCrtcs->randr_crtc, pVBox->pScreens[i].paOutputs->randr_output->modes[0], x, 0,
                      RR_Rotate_0, 1, &pVBox->pScreens[i].paOutputs->randr_output);
        if (pVBox->pScreens[i].paOutputs->status == XF86OutputStatusConnected)
            x += pVBox->pScreens[i].aPreferredSize.cx;
    }
#else /* !VBOXVIDEO_13 */
    pNewMode = pScrn->modes != pScrn->currentMode ? pScrn->modes : pScrn->modes->next;
    pNewMode->HDisplay = pVBox->pScreens[0].aPreferredSize.cx;
    pNewMode->VDisplay = pVBox->pScreens[0].aPreferredSize.cy;
#endif
    vbvxReprobeCursor(pScrn);
}

/* We update the size hints from the X11 property set by VBoxClient every time
 * that the X server goes to sleep (to catch the property change request).
 * Although this is far more often than necessary it should not have real-life
 * performance consequences and allows us to simplify the code quite a bit. */
static void updateSizeHintsBlockHandler(pointer pData, OSTimePtr pTimeout, pointer pReadmask)
{
    ScrnInfoPtr pScrn = (ScrnInfoPtr)pData;
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    bool fNeedUpdate;

    (void)pTimeout;
    (void)pReadmask;
    if (!pScrn->vtSema || ROOT_WINDOW(pScrn) == NULL)
        return;
    vbvxReadSizesAndCursorIntegrationFromProperties(pScrn, &fNeedUpdate);
    if (fNeedUpdate || pVBox->fForceModeUpdate)
        setSizesAndCursorIntegration(pScrn, false);
}

/* We update the size hints from the host every time that the X server is woken
 * up by a client.  Although this is far more often than necessary it should
 * not have real-life performance consequences and allows us to simplify the
 * code quite a bit. */
static void updateSizeHintsWakeupHandler(pointer pData, int i, pointer pLastSelectMask)
{
#ifdef VBOXVIDEO_13
    ScrnInfoPtr pScrn = (ScrnInfoPtr)pData;
    bool fNeedUpdate;

    (void)i;
    (void)pLastSelectMask;
    if (!pScrn->vtSema || ROOT_WINDOW(pScrn) == NULL)
        return;
    vbvxReadSizesAndCursorIntegrationFromHGSMI(pScrn, &fNeedUpdate);
    if (fNeedUpdate)
        setSizesAndCursorIntegration(pScrn, false);
#else
    (void)pData; (void)i; (void)pLastSelectMask;
#endif
}

/*
 * QUOTE from the XFree86 DESIGN document:
 *
 * This is called at the start of each server generation.
 *
 * (...)
 *
 * Decide which operations need to be placed under resource access
 * control. (...) Map any video memory or other memory regions. (...)
 * Save the video card state. (...) Initialise the initial video
 * mode.
 *
 * End QUOTE.
 */
static Bool VBOXScreenInit(ScreenPtr pScreen, int argc, char **argv)
{
    ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    VisualPtr visual;
    unsigned flags;

    TRACE_ENTRY();

    if (!VBOXMapVidMem(pScrn))
        return (FALSE);

    /* save current video state */
    VBOXSaveMode(pScrn);

    /* mi layer - reset the visual list (?)*/
    miClearVisualTypes();
    if (!miSetVisualTypes(pScrn->depth, TrueColorMask,
                          pScrn->rgbBits, TrueColor))
        return (FALSE);
    if (!miSetPixmapDepths())
        return (FALSE);

#ifdef VBOX_DRI
    pVBox->useDRI = VBOXDRIScreenInit(pScrn, pScreen, pVBox);
# ifndef VBOX_DRI_OLD  /* DRI2 */
    if (pVBox->drmFD >= 0)
        /* Tell the kernel driver, if present, that we are taking over. */
        drmIoctl(pVBox->drmFD, VBOXVIDEO_IOCTL_DISABLE_HGSMI, NULL);
# endif
#endif

    if (!fbScreenInit(pScreen, pVBox->base,
                      pScrn->virtualX, pScrn->virtualY,
                      pScrn->xDpi, pScrn->yDpi,
                      pScrn->displayWidth, pScrn->bitsPerPixel))
        return (FALSE);

    /* Fixup RGB ordering */
    /** @note the X server uses this even in true colour. */
    visual = pScreen->visuals + pScreen->numVisuals;
    while (--visual >= pScreen->visuals) {
        if ((visual->class | DynamicClass) == DirectColor) {
            visual->offsetRed   = pScrn->offset.red;
            visual->offsetGreen = pScrn->offset.green;
            visual->offsetBlue  = pScrn->offset.blue;
            visual->redMask     = pScrn->mask.red;
            visual->greenMask   = pScrn->mask.green;
            visual->blueMask    = pScrn->mask.blue;
        }
    }

    /* must be after RGB ordering fixed */
    fbPictureInit(pScreen, 0, 0);

    xf86SetBlackWhitePixels(pScreen);
    pScrn->vtSema = TRUE;

    if (!VBoxHGSMIIsSupported())
    {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Graphics device too old to support.\n");
        return FALSE;
    }
    vbvxSetUpHGSMIHeapInGuest(pVBox, pScrn->videoRam * 1024);
    pVBox->cScreens = VBoxHGSMIGetMonitorCount(&pVBox->guestCtx);
    pVBox->pScreens = xnfcalloc(pVBox->cScreens, sizeof(*pVBox->pScreens));
    pVBox->paVBVAModeHints = xnfcalloc(pVBox->cScreens, sizeof(*pVBox->paVBVAModeHints));
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Requested monitor count: %u\n", pVBox->cScreens);
    vboxEnableVbva(pScrn);
    /* Set up the dirty rectangle handler.  It will be added into a function
     * chain and gets removed when the screen is cleaned up. */
    if (ShadowFBInit2(pScreen, NULL, vbvxHandleDirtyRect) != TRUE)
        return FALSE;
    VBoxInitialiseSizeHints(pScrn);
    /* Get any screen size hints from HGSMI.  Do not yet try to access X11
     * properties, as they are not yet set up, and nor are the clients that
     * might have set them. */
    vbvxReadSizesAndCursorIntegrationFromHGSMI(pScrn, NULL);

#ifdef VBOXVIDEO_13
    /* Initialise CRTC and output configuration for use with randr1.2. */
    xf86CrtcConfigInit(pScrn, &VBOXCrtcConfigFuncs);

    {
        uint32_t i;

        for (i = 0; i < pVBox->cScreens; ++i)
        {
            char szOutput[256];

            /* Setup our virtual CRTCs. */
            pVBox->pScreens[i].paCrtcs = xf86CrtcCreate(pScrn, &VBOXCrtcFuncs);
            pVBox->pScreens[i].paCrtcs->driver_private = (void *)(uintptr_t)i;

            /* Set up our virtual outputs. */
            snprintf(szOutput, sizeof(szOutput), "VGA-%u", i);
            pVBox->pScreens[i].paOutputs
                = xf86OutputCreate(pScrn, &VBOXOutputFuncs, szOutput);

            /* We are not interested in the monitor section in the
             * configuration file. */
            xf86OutputUseScreenMonitor(pVBox->pScreens[i].paOutputs, FALSE);
            pVBox->pScreens[i].paOutputs->possible_crtcs = 1 << i;
            pVBox->pScreens[i].paOutputs->possible_clones = 0;
            pVBox->pScreens[i].paOutputs->driver_private = (void *)(uintptr_t)i;
            TRACE_LOG("Created crtc (%p) and output %s (%p)\n",
                      (void *)pVBox->pScreens[i].paCrtcs, szOutput,
                      (void *)pVBox->pScreens[i].paOutputs);
        }
    }

    /* Set a sane minimum and maximum mode size to match what the hardware
     * supports. */
    xf86CrtcSetSizeRange(pScrn, 64, 64, VBE_DISPI_MAX_XRES, VBE_DISPI_MAX_YRES);

    /* Now create our initial CRTC/output configuration. */
    if (!xf86InitialConfiguration(pScrn, TRUE)) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Initial CRTC configuration failed!\n");
        return (FALSE);
    }

    /* Initialise randr 1.2 mode-setting functions. */
    if (!xf86CrtcScreenInit(pScreen)) {
        return FALSE;
    }

    setSizesAndCursorIntegration(pScrn, true);
#else /* !VBOXVIDEO_13 */
    /* set first video mode */
    if (!VBOXSwitchMode(pScrn, pScrn->currentMode))
        return FALSE;
#endif

    /* Register block and wake-up handlers for getting new screen size hints. */
    RegisterBlockAndWakeupHandlers(updateSizeHintsBlockHandler, updateSizeHintsWakeupHandler, (pointer)pScrn);

    /* software cursor */
    miDCInitialize(pScreen, xf86GetPointerScreenFuncs());

    /* colourmap code */
    if (!miCreateDefColormap(pScreen))
        return (FALSE);

    if(!xf86HandleColormaps(pScreen, 256, 8, vboxLoadPalette, NULL, 0))
        return (FALSE);

    pVBox->CloseScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = SCRNINDEXAPI(VBOXCloseScreen);
#ifdef VBOXVIDEO_13
    pScreen->SaveScreen = xf86SaveScreen;
#else
    pScreen->SaveScreen = VBOXSaveScreen;
#endif

#ifdef VBOXVIDEO_13
    xf86DPMSInit(pScreen, xf86DPMSSet, 0);
#else
    /* We probably do want to support power management - even if we just use
       a dummy function. */
    xf86DPMSInit(pScreen, VBOXDisplayPowerManagementSet, 0);
#endif

    /* Report any unused options (only for the first generation) */
    if (serverGeneration == 1)
        xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);

    if (vbox_cursor_init(pScreen) != TRUE)
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "Unable to start the VirtualBox mouse pointer integration with the host system.\n");

#ifdef VBOX_DRI_OLD
    if (pVBox->useDRI)
        pVBox->useDRI = VBOXDRIFinishScreenInit(pScreen);
#endif

    initialiseProperties(pScrn);

    return (TRUE);
}

static Bool VBOXEnterVT(ScrnInfoPtr pScrn)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);

    TRACE_ENTRY();
#ifdef VBOX_DRI_OLD
    if (pVBox->useDRI)
        DRIUnlock(xf86ScrnToScreen(pScrn));
#elif defined(VBOX_DRI)  /* DRI2 */
    if (pVBox->drmFD >= 0)
    {
        /* Tell the kernel driver, if present, that we are taking over. */
        drmSetMaster(pVBox->drmFD);
    }
#endif
    vbvxSetUpHGSMIHeapInGuest(pVBox, pScrn->videoRam * 1024);
    vboxEnableVbva(pScrn);
    /* Re-set video mode */
    vbvxReadSizesAndCursorIntegrationFromHGSMI(pScrn, NULL);
    vbvxReadSizesAndCursorIntegrationFromProperties(pScrn, NULL);
    setSizesAndCursorIntegration(pScrn, false);
#ifdef SET_HAVE_VT_PROPERTY
    updateHasVTProperty(pScrn, TRUE);
#endif
    return TRUE;
}

static void VBOXLeaveVT(ScrnInfoPtr pScrn)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);
#ifdef VBOXVIDEO_13
    unsigned i;
#endif

    TRACE_ENTRY();
#ifdef VBOXVIDEO_13
    for (i = 0; i < pVBox->cScreens; ++i)
        vbox_output_dpms(pVBox->pScreens[i].paOutputs, DPMSModeOff);
#endif
    vboxDisableVbva(pScrn);
    vbvxClearVRAM(pScrn, pScrn->virtualX * pScrn->virtualY * pScrn->bitsPerPixel, 0);
#ifdef VBOX_DRI_OLD
    if (pVBox->useDRI)
        DRILock(xf86ScrnToScreen(pScrn), 0);
#elif defined(VBOX_DRI)  /* DRI2 */
    if (pVBox->drmFD >= 0)
        drmDropMaster(pVBox->drmFD);
#endif
    VBOXRestoreMode(pScrn);
#ifdef SET_HAVE_VT_PROPERTY
    updateHasVTProperty(pScrn, FALSE);
#endif
    TRACE_EXIT();
}

static Bool VBOXCloseScreen(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
    VBOXPtr pVBox = VBOXGetRec(pScrn);
#if defined(VBOX_DRI) && !defined(VBOX_DRI_OLD)  /* DRI2 */
    BOOL fRestore = TRUE;
#endif
    if (pScrn->vtSema)
    {
#ifdef VBOXVIDEO_13
        unsigned i;

        for (i = 0; i < pVBox->cScreens; ++i)
            vbox_output_dpms(pVBox->pScreens[i].paOutputs, DPMSModeOff);
#endif
        vboxDisableVbva(pScrn);
        vbvxClearVRAM(pScrn, pScrn->virtualX * pScrn->virtualY * pScrn->bitsPerPixel, 0);
    }
#ifdef VBOX_DRI
# ifndef VBOX_DRI_OLD  /* DRI2 */
    if (   pVBox->drmFD >= 0
        /* Tell the kernel driver, if present, that we are going away. */
        && drmIoctl(pVBox->drmFD, VBOXVIDEO_IOCTL_ENABLE_HGSMI, NULL) >= 0)
        fRestore = false;
# endif
    if (pVBox->useDRI)
        VBOXDRICloseScreen(pScreen, pVBox);
    pVBox->useDRI = false;
#endif
#if defined(VBOX_DRI) && !defined(VBOX_DRI_OLD)  /* DRI2 */
    if (fRestore)
#endif
        if (pScrn->vtSema)
            VBOXRestoreMode(pScrn);
    if (pScrn->vtSema)
        VBOXUnmapVidMem(pScrn);
    pScrn->vtSema = FALSE;

    /* Do additional bits which are separate for historical reasons */
    vbox_close(pScrn, pVBox);

    pScreen->CloseScreen = pVBox->CloseScreen;
#ifndef XF86_SCRN_INTERFACE
    return pScreen->CloseScreen(pScreen->myNum, pScreen);
#else
    return pScreen->CloseScreen(pScreen);
#endif
}

static Bool VBOXSwitchMode(ScrnInfoPtr pScrn, DisplayModePtr pMode)
{
    VBOXPtr pVBox;
    Bool rc = TRUE;

    TRACE_LOG("HDisplay=%d, VDisplay=%d\n", pMode->HDisplay, pMode->VDisplay);
    if (!pScrn->vtSema)
    {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "We do not own the active VT, exiting.\n");
        return TRUE;
    }
#ifdef VBOXVIDEO_13
    rc = xf86SetSingleMode(pScrn, pMode, RR_Rotate_0);
#else
    setModeRandR11(pScrn, pMode);
#endif
    TRACE_LOG("returning %s\n", rc ? "TRUE" : "FALSE");
    return rc;
}

static void VBOXAdjustFrame(ScrnInfoPtr pScrn, int x, int y)
{ (void)pScrn; (void)x; (void)y; }

static void VBOXFreeScreen(ScrnInfoPtr pScrn)
{
    /* Destroy the VGA hardware record */
    vgaHWFreeHWRec(pScrn);
    /* And our private record */
    free(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;
}

static Bool
VBOXMapVidMem(ScrnInfoPtr pScrn)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    Bool rc = TRUE;

    TRACE_ENTRY();
    if (!pVBox->base)
    {
#ifdef PCIACCESS
        (void) pci_device_map_range(pVBox->pciInfo,
                                    pScrn->memPhysBase,
                                    pScrn->videoRam * 1024,
                                    PCI_DEV_MAP_FLAG_WRITABLE,
                                    & pVBox->base);
#else
        pVBox->base = xf86MapPciMem(pScrn->scrnIndex,
                                    VIDMEM_FRAMEBUFFER,
                                    pVBox->pciTag, pScrn->memPhysBase,
                                    (unsigned) pScrn->videoRam * 1024);
#endif
        if (!pVBox->base)
            rc = FALSE;
    }
    TRACE_LOG("returning %s\n", rc ? "TRUE" : "FALSE");
    return rc;
}

static void
VBOXUnmapVidMem(ScrnInfoPtr pScrn)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);

    TRACE_ENTRY();
    if (pVBox->base == NULL)
        return;

#ifdef PCIACCESS
    (void) pci_device_unmap_range(pVBox->pciInfo,
                                  pVBox->base,
                                  pScrn->videoRam * 1024);
#else
    xf86UnMapVidMem(pScrn->scrnIndex, pVBox->base,
                    (unsigned) pScrn->videoRam * 1024);
#endif
    pVBox->base = NULL;
    TRACE_EXIT();
}

static Bool
VBOXSaveScreen(ScreenPtr pScreen, int mode)
{
    (void)pScreen; (void)mode;
    return TRUE;
}

void
VBOXSaveMode(ScrnInfoPtr pScrn)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    vgaRegPtr vgaReg;

    TRACE_ENTRY();
    vgaReg = &VGAHWPTR(pScrn)->SavedReg;
    vgaHWSave(pScrn, vgaReg, VGA_SR_ALL);
    pVBox->fSavedVBEMode = VBoxVideoGetModeRegisters(&pVBox->cSavedWidth,
                                                     &pVBox->cSavedHeight,
                                                     &pVBox->cSavedPitch,
                                                     &pVBox->cSavedBPP,
                                                     &pVBox->fSavedFlags);
}

void
VBOXRestoreMode(ScrnInfoPtr pScrn)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    vgaRegPtr vgaReg;
#ifdef VBOX_DRI
    drmModeResPtr pRes;
#endif

    TRACE_ENTRY();
#ifdef VBOX_DRI
    /* Do not try to re-set the VGA state if a mode-setting driver is loaded. */
    if (   pVBox->drmFD >= 0
        && LoaderSymbol("drmModeGetResources") != NULL
        && (pRes = drmModeGetResources(pVBox->drmFD)) != NULL)
    {
        drmModeFreeResources(pRes);
        return;
    }
#endif
    vgaReg = &VGAHWPTR(pScrn)->SavedReg;
    vgaHWRestore(pScrn, vgaReg, VGA_SR_ALL);
    if (pVBox->fSavedVBEMode)
        VBoxVideoSetModeRegisters(pVBox->cSavedWidth, pVBox->cSavedHeight,
                                  pVBox->cSavedPitch, pVBox->cSavedBPP,
                                  pVBox->fSavedFlags, 0, 0);
    else
        VBoxVideoDisableVBE();
}

static void
VBOXDisplayPowerManagementSet(ScrnInfoPtr pScrn, int mode,
                int flags)
{
    (void)pScrn; (void)mode; (void) flags;
}
