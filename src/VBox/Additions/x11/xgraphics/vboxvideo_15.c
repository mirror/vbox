/** @file
 *
 * Linux Additions X11 graphics driver
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 * --------------------------------------------------------------------
 *
 * This code is based on:
 *
 * X11 VESA driver
 *
 * Copyright (c) 2000 by Conectiva S.A. (http://www.conectiva.com)
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
 */

#ifdef DEBUG_michael
# define DEBUG_VIDEO 1
#endif

#ifdef DEBUG_VIDEO

#define TRACE \
do { \
    xf86Msg(X_INFO, __PRETTY_FUNCTION__); \
    xf86Msg(X_INFO, ": entering\n"); \
} while(0)
#define TRACE2 \
do { \
    xf86Msg(X_INFO, __PRETTY_FUNCTION__); \
    xf86Msg(X_INFO, ": leaving\n"); \
} while(0)
#define TRACE3(...) \
do { \
    xf86Msg(X_INFO, __PRETTY_FUNCTION__); \
    xf86Msg(X_INFO, __VA_ARGS__); \
} while(0)

#else  /* DEBUG_VIDEO not defined */

#define TRACE       do { } while(0)
#define TRACE2      do { } while(0)
#define TRACE3(...) do { } while(0)

#endif  /* DEBUG_VIDEO not defined */

#define BOOL_STR(a) ((a) ? "TRUE" : "FALSE")

#ifdef XFree86LOADER
# include "xorg-server.h"
#else
# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif
#endif
#include "vboxvideo.h"
#include "version-generated.h"
#include <xf86.h>

/* All drivers initialising the SW cursor need this */
#include "mipointer.h"

/* All drivers implementing backing store need this */
#include "mibstore.h"

/* Colormap handling */
#include "micmap.h"
#include "xf86cmap.h"

/* DPMS */
/* #define DPMS_SERVER
#include "extensions/dpms.h" */

/* X.org 1.3+ mode setting */
#include "xf86Crtc.h"
#include "xf86Modes.h"

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
static Bool VBOXScreenInit(int Index, ScreenPtr pScreen, int argc,
                           char **argv);
static Bool VBOXEnterVT(int scrnIndex, int flags);
static void VBOXLeaveVT(int scrnIndex, int flags);
static Bool VBOXCloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool VBOXSwitchMode(int scrnIndex, DisplayModePtr pMode, int flags);
static ModeStatus VBOXValidMode(int scrn, DisplayModePtr p, Bool flag, int pass);
static Bool VBOXSetMode(ScrnInfoPtr pScrn, DisplayModePtr pMode);
static void VBOXAdjustFrame(int scrnIndex, int x, int y, int flags);
static void VBOXFreeScreen(int scrnIndex, int flags);
static void VBOXFreeRec(ScrnInfoPtr pScrn);

/* locally used functions */
static Bool VBOXMapVidMem(ScrnInfoPtr pScrn);
static void VBOXUnmapVidMem(ScrnInfoPtr pScrn);
static void VBOXLoadPalette(ScrnInfoPtr pScrn, int numColors,
                            int *indices,
                            LOCO *colors, VisualPtr pVisual);
static void SaveFonts(ScrnInfoPtr pScrn);
static void RestoreFonts(ScrnInfoPtr pScrn);
static Bool VBOXSaveRestore(ScrnInfoPtr pScrn,
                            vbeSaveRestoreFunction function);

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
    {-1,	 NULL}
};

static PciChipsets VBOXPCIchipsets[] = {
  { VBOX_DEVICEID, VBOX_DEVICEID, RES_SHARED_VGA },
  { -1,		-1,		    RES_UNDEFINED },
};

/*
 * This contains the functions needed by the server after loading the
 * driver module.  It must be supplied, and gets added the driver list by
 * the Module Setup funtion in the dynamic case.  In the static case a
 * reference to this is compiled in, and this requires that the name of
 * this DriverRec be an upper-case version of the driver name.
 */

_X_EXPORT DriverRec VBOXDRV = {
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
    NULL,

#ifdef PCIACCESS
    vbox_device_match,
    VBOXPciProbe
#endif
};

/* No options for now */
static const OptionInfoRec VBOXOptions[] = {
    { -1,		NULL,		OPTV_NONE,	{0},	FALSE }
};

static VBOXPtr
VBOXGetRec(ScrnInfoPtr pScrn)
{
    if (!pScrn->driverPrivate) {
        pScrn->driverPrivate = xcalloc(sizeof(VBOXRec), 1);
    }

    return ((VBOXPtr)pScrn->driverPrivate);
}

static void
VBOXFreeRec(ScrnInfoPtr pScrn)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    xfree(pVBox->savedPal);
    xfree(pVBox->fonts);
    xfree(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;
}

/* X.org 1.3+ mode-setting support ******************************************/

/* For descriptions of these functions and structures, see
   hw/xfree86/modes/xf86Crtc.h and hw/xfree86/modes/xf86Modes.h in the
   X.Org source tree. */

static Bool
VBOXCrtcResize(ScrnInfoPtr scrn, int width, int height)
{
    int bpp = scrn->bitsPerPixel;
    ScreenPtr pScreen = scrn->pScreen;
    PixmapPtr pPixmap = NULL;
    VBOXPtr pVBox = VBOXGetRec(scrn);
    Bool rc = TRUE;

    TRACE3("width=%d, height=%d\n", width, height);
    /* We only support horizontal resolutions which are a multiple of 8.  Round down if
       necessary. */
    if (width % 8 != 0)
    {
        xf86DrvMsg(scrn->scrnIndex, X_WARNING,
                   "VirtualBox only supports virtual screen widths which are a multiple of 8.  Rounding down from %d to %d\n",
                   width, width - (width % 8));
        width = width - (width % 8);
    }
    if (width * height * bpp / 8 >= scrn->videoRam * 1024)
    {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR,
                   "Unable to set up a virtual screen size of %dx%d with %d Kb of video memory.  Please increase the video memory size.\n",
                   width, height, scrn->videoRam);
        rc = FALSE;
    }
    if (rc) {
        pPixmap = pScreen->GetScreenPixmap(pScreen);
        if (NULL == pPixmap) {
            xf86DrvMsg(scrn->scrnIndex, X_ERROR,
                       "Failed to get the screen pixmap.\n");
            rc = FALSE;
        }
    }
    if (rc) {
        if (
            !pScreen->ModifyPixmapHeader(pPixmap, width, height,
                                         scrn->depth, bpp, width * bpp / 8,
                                         pVBox->base)
           ) {
            xf86DrvMsg(scrn->scrnIndex, X_ERROR,
                       "Failed to set up the screen pixmap.\n");
            rc = FALSE;
        }
    }
    if (rc) {
        scrn->virtualX = width;
        scrn->virtualY = height;
        scrn->displayWidth = width;
    }
    TRACE3("returning %s\n", rc ? "TRUE" : "FALSE");
    return rc;
}

static const xf86CrtcConfigFuncsRec VBOXCrtcConfigFuncs = {
    VBOXCrtcResize
};

static void
vbox_crtc_dpms(xf86CrtcPtr crtc, int mode)
{ (void) crtc; (void) mode; }

static Bool
vbox_crtc_lock (xf86CrtcPtr crtc)
{ (void) crtc; return FALSE; }

static Bool
vbox_crtc_mode_fixup (xf86CrtcPtr crtc, DisplayModePtr mode,
                      DisplayModePtr adjusted_mode)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    int xRes = adjusted_mode->HDisplay;

    (void) mode;
    TRACE3("name=%s, HDisplay=%d, VDisplay=%d\n", adjusted_mode->name,
           adjusted_mode->HDisplay, adjusted_mode->VDisplay);
    /* We only support horizontal resolutions which are a multiple of 8.  Round down if
       necessary. */
    if (xRes % 8 != 0)
    {
        xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                   "VirtualBox only supports screen widths which are a multiple of 8.  Rounding down from %d to %d\n",
                   xRes, xRes - (xRes % 8));
        adjusted_mode->HDisplay = xRes - (xRes % 8);
    }
    return TRUE;
}

static void
vbox_crtc_stub (xf86CrtcPtr crtc)
{ (void) crtc; }

static void
vbox_crtc_mode_set (xf86CrtcPtr crtc, DisplayModePtr mode,
                    DisplayModePtr adjusted_mode, int x, int y)
{
    (void) mode;
    TRACE3("name=%s, HDisplay=%d, VDisplay=%d, x=%d, y=%d\n", adjusted_mode->name,
           adjusted_mode->HDisplay, adjusted_mode->VDisplay, x, y);
    VBOXSetMode(crtc->scrn, adjusted_mode);
    VBOXAdjustFrame(crtc->scrn->scrnIndex, x, y, 0);
    vboxSaveVideoMode(crtc->scrn, adjusted_mode->HDisplay,
                      adjusted_mode->VDisplay, crtc->scrn->bitsPerPixel);
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
{ (void) output; (void) mode; }

static int
vbox_output_mode_valid (xf86OutputPtr output, DisplayModePtr mode)
{
    ScrnInfoPtr pScrn = output->scrn;
    int rc = MODE_OK;
    TRACE3("HDisplay=%d, VDisplay=%d\n", mode->HDisplay, mode->VDisplay);
    /* We always like modes specified by the user in the configuration
     * file, as doing otherwise is likely to annoy people. */
    if (   !(mode->type & M_T_USERDEF)
        && vbox_device_available(VBOXGetRec(pScrn))
        && !vboxHostLikesVideoMode(pScrn, mode->HDisplay, mode->VDisplay,
                                   pScrn->bitsPerPixel)
       )
        rc = MODE_BAD;
    TRACE3("returning %s\n", MODE_OK == rc ? "MODE_OK" : "MODE_BAD");
    return rc;
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
    (void) output;
    return XF86OutputStatusConnected;
}

static void
vbox_output_add_mode (DisplayModePtr *pModes, const char *pszName, int x, int y,
                      Bool isPreferred, Bool isUserDef)
{
    TRACE3("pszName=%s, x=%d, y=%d\n", pszName, x, y);
    DisplayModePtr pMode = xnfcalloc(1, sizeof(DisplayModeRec));

    pMode->status        = MODE_OK;
    /* We don't ask the host whether it likes user defined modes,
     * as we assume that the user really wanted that mode. */
    pMode->type          = isUserDef ? M_T_USERDEF : M_T_BUILTIN;
    if (isPreferred)
        pMode->type     |= M_T_PREFERRED;
    /* VBox only supports screen widths which are a multiple of 8 */
    pMode->HDisplay      = (x + 7) & ~7;
    pMode->HSyncStart    = pMode->HDisplay + 2;
    pMode->HSyncEnd      = pMode->HDisplay + 4;
    pMode->HTotal        = pMode->HDisplay + 6;
    pMode->VDisplay      = y;
    pMode->VSyncStart    = pMode->VDisplay + 2;
    pMode->VSyncEnd      = pMode->VDisplay + 4;
    pMode->VTotal        = pMode->VDisplay + 6;
    pMode->Clock         = pMode->HTotal * pMode->VTotal * 60 / 1000; /* kHz */
    if (NULL == pszName) {
        xf86SetModeDefaultName(pMode);
    } else {
        pMode->name          = xnfstrdup(pszName);
    }
    *pModes = xf86ModesAdd(*pModes, pMode);
}

static DisplayModePtr
vbox_output_get_modes (xf86OutputPtr output)
{
    bool rc;
    unsigned i;
    DisplayModePtr pModes = NULL;
    ScrnInfoPtr pScrn = output->scrn;
    VBOXPtr pVBox = VBOXGetRec(pScrn);

    TRACE;
    if (vbox_device_available(pVBox))
    {
        uint32_t x, y, bpp, display;
        rc = vboxGetDisplayChangeRequest(pScrn, &x, &y, &bpp, &display);
        /* @todo - check the display number once we support multiple displays. */
        /* If we don't find a display request, see if we have a saved hint
         * from a previous session. */
        if (!rc || (0 == x) || (0 == y))
            rc = vboxRetrieveVideoMode(pScrn, &x, &y, &bpp);
        if (rc && (0 != x) && (0 != y)) {
            /* We prefer a slightly smaller size to a slightly larger one */
            x -= (x % 8);
            vbox_output_add_mode(&pModes, NULL, x, y, TRUE, FALSE);
        }
    }
    /* Also report any modes the user may have requested in the xorg.conf
     * configuration file. */
    for (i = 0; pScrn->display->modes[i] != NULL; i++)
    {
        int x, y;
        if (2 == sscanf(pScrn->display->modes[i], "%dx%d", &x, &y))
            vbox_output_add_mode(&pModes, pScrn->display->modes[i], x, y,
                                 FALSE, TRUE);
    }
    TRACE2;
    return pModes;
}

#ifdef RANDR_12_INTERFACE
/* We don't yet have mutable properties, whatever they are. */
static Bool
vbox_output_set_property(xf86OutputPtr output, Atom property,
                         RRPropertyValuePtr value)
{ (void) output, (void) property, (void) value; return FALSE; }
#endif

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
     .set_property = vbox_output_set_property,
#endif
    .destroy = vbox_output_stub
};

/*
 * List of symbols from other modules that this module references.  This
 * list is used to tell the loader that it is OK for symbols here to be
 * unresolved providing that it hasn't been told that they are essential
 * via a call to xf86LoaderReqSymbols() or xf86LoaderReqSymLists().  The
 * purpose is this is to avoid warnings about unresolved symbols that are
 * not required.
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

static const char *vbeSymbols[] = {
    "VBEExtendedInit",
    "VBEFindSupportedDepths",
    "VBEGetModeInfo",
    "VBEGetVBEInfo",
    "VBEGetVBEMode",
    "VBEPrintModes",
    "VBESaveRestore",
    "VBESetDisplayStart",
    "VBESetGetDACPaletteFormat",
    "VBESetGetLogicalScanlineLength",
    "VBESetGetPaletteData",
    "VBESetModeNames",
    "VBESetModeParameters",
    "VBESetVBEMode",
    "VBEValidateModes",
    "vbeDoEDID",
    "vbeFree",
    NULL
};

static const char *ramdacSymbols[] = {
    "xf86InitCursor",
    "xf86CreateCursorInfoRec",
    NULL
};

#ifdef XFree86LOADER
/* Module loader interface */
static MODULESETUPPROTO(vboxSetup);

static XF86ModuleVersionInfo vboxVersionRec =
{
    VBOX_DRIVER_NAME,
    "Sun Microsystems, Inc.",
    MODINFOSTRING1,
    MODINFOSTRING2,
    XORG_VERSION_CURRENT,
    1,                          /* Module major version. Xorg-specific */
    0,                          /* Module minor version. Xorg-specific */
    1,                          /* Module patchlevel. Xorg-specific */
    ABI_CLASS_VIDEODRV,	        /* This is a video driver */
    ABI_VIDEODRV_VERSION,
    MOD_CLASS_VIDEODRV,
    {0, 0, 0, 0}
};

/*
 * This data is accessed by the loader.  The name must be the module name
 * followed by "ModuleData".
 */
_X_EXPORT XF86ModuleData vboxvideoModuleData = { &vboxVersionRec, vboxSetup, NULL };

static pointer
vboxSetup(pointer Module, pointer Options, int *ErrorMajor, int *ErrorMinor)
{
    static Bool Initialised = FALSE;

    if (!Initialised)
    {
        Initialised = TRUE;
#ifdef PCIACCESS
        xf86AddDriver(&VBOXDRV, Module, HaveDriverFuncs);
#else
        xf86AddDriver(&VBOXDRV, Module, 0);
#endif
        LoaderRefSymLists(fbSymbols,
                          shadowfbSymbols,
                          vbeSymbols,
                          ramdacSymbols,
                          NULL);
        return (pointer)TRUE;
    }

    if (ErrorMajor)
        *ErrorMajor = LDR_ONCEONLY;
    return (NULL);
}

#endif  /* XFree86Loader defined */

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

/*
 * This function is called once, at the start of the first server generation to
 * do a minimal probe for supported hardware.
 */

#ifdef PCIACCESS
static Bool
VBOXPciProbe(DriverPtr drv, int entity_num, struct pci_device *dev,
             intptr_t match_data)
{
    ScrnInfoPtr pScrn;

    TRACE;
    pScrn = xf86ConfigPciEntity(NULL, 0, entity_num, VBOXPCIchipsets,
                                NULL, NULL, NULL, NULL, NULL);
    if (pScrn != NULL) {
        VBOXPtr pVBox = VBOXGetRec(pScrn);

        pScrn->driverVersion = VBOX_VERSION;
        pScrn->driverName    = VBOX_DRIVER_NAME;
        pScrn->name          = VBOX_NAME;
        pScrn->Probe         = NULL;
        pScrn->PreInit       = VBOXPreInit;
        pScrn->ScreenInit    = VBOXScreenInit;
        pScrn->SwitchMode    = VBOXSwitchMode;
        pScrn->ValidMode     = VBOXValidMode;
        pScrn->AdjustFrame   = VBOXAdjustFrame;
        pScrn->EnterVT       = VBOXEnterVT;
        pScrn->LeaveVT       = VBOXLeaveVT;
        pScrn->FreeScreen    = VBOXFreeScreen;

        pVBox->pciInfo = dev;
    }

    TRACE3("returning %s\n", BOOL_STR(pScrn != NULL));
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
    if (xf86GetPciVideoInfo()) {
        int numUsed;
        int *usedChips;
        int i;
	numUsed = xf86MatchPciInstances(VBOX_NAME, VBOX_VENDORID,
					VBOXChipsets, VBOXPCIchipsets,
					devSections, numDevSections,
					drv, &usedChips);
	if (numUsed > 0) {
	    if (flags & PROBE_DETECT)
		foundScreen = TRUE;
	    else {
		for (i = 0; i < numUsed; i++) {
		    ScrnInfoPtr pScrn = NULL;
		    /* Allocate a ScrnInfoRec  */
		    if ((pScrn = xf86ConfigPciEntity(pScrn,0,usedChips[i],
						     VBOXPCIchipsets,NULL,
						     NULL,NULL,NULL,NULL))) {
			pScrn->driverVersion = VBOX_VERSION;
			pScrn->driverName    = VBOX_DRIVER_NAME;
			pScrn->name	     = VBOX_NAME;
			pScrn->Probe	     = VBOXProbe;
                        pScrn->PreInit       = VBOXPreInit;
			pScrn->ScreenInit    = VBOXScreenInit;
			pScrn->SwitchMode    = VBOXSwitchMode;
			pScrn->ValidMode     = VBOXValidMode;
			pScrn->AdjustFrame   = VBOXAdjustFrame;
			pScrn->EnterVT       = VBOXEnterVT;
			pScrn->LeaveVT       = VBOXLeaveVT;
			pScrn->FreeScreen    = VBOXFreeScreen;
			foundScreen = TRUE;
		    }
		}
	    }
	    xfree(usedChips);
	}
    }

    xfree(devSections);

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
    xf86OutputPtr output;

    /* Are we really starting the server, or is this just a dummy run? */
    if (flags & PROBE_DETECT)
        return (FALSE);

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
               "VirtualBox guest additions video driver version "
               VBOX_VERSION_STRING "\n");

    /* Get our private data from the ScrnInfoRec structure. */
    pVBox = VBOXGetRec(pScrn);

    /* Initialise the guest library */
    vbox_init(pScrn->scrnIndex, pVBox);

    /* Entity information seems to mean bus information. */
    pVBox->pEnt = xf86GetEntityInfo(pScrn->entityList[0]);

    /* We need the vbe module because we use VBE code to save and restore
       text mode, in order to keep our code simple. */
    if (!xf86LoadSubModule(pScrn, "vbe"))
        return (FALSE);
    xf86LoaderReqSymLists(vbeSymbols, NULL);

    if ((pVBox->pVbe = VBEExtendedInit(NULL, pVBox->pEnt->index,
                                       SET_BIOS_SCRATCH
                                       | RESTORE_BIOS_SCRATCH)) == NULL)
        return (FALSE);

#ifndef PCIACCESS
    if (pVBox->pEnt->location.type != BUS_PCI)
        return FALSE;

    pVBox->pciInfo = xf86GetPciInfoForEntity(pVBox->pEnt->index);
    pVBox->pciTag = pciTag(pVBox->pciInfo->bus,
                           pVBox->pciInfo->device,
                           pVBox->pciInfo->func);
#endif

    /* The ramdac module is needed for the hardware cursor. */
    if (!xf86LoadSubModule(pScrn, "ramdac"))
        return FALSE;
    xf86LoaderReqSymLists(ramdacSymbols, NULL);

    /* The framebuffer module. */
    if (xf86LoadSubModule(pScrn, "fb") == NULL)
        return (FALSE);
    xf86LoaderReqSymLists(fbSymbols, NULL);

    if (!xf86LoadSubModule(pScrn, "shadowfb"))
        return FALSE;
    xf86LoaderReqSymLists(shadowfbSymbols, NULL);

    /* Set up our ScrnInfoRec structure to describe our virtual
       capabilities to X. */

    pScrn->chipset = "vbox";

    /* I assume that this is no longer a requirement in the config file. */
    pScrn->monitor = pScrn->confScreen->monitor;

    pScrn->progClock = TRUE;
    pScrn->rgbBits = 8;

    /* Using the PCI information caused problems with non-powers-of-two
       sized video RAM configurations */
    pScrn->videoRam = inl(VBE_DISPI_IOPORT_DATA) / 1024;

    /* Query the host for the preferred colour depth */
    {
        uint32_t cx, cy, iDisplay, cBits = 24;

        if (vbox_device_available(pVBox))
        {
            /* We only support 16 and 24 bits depth (i.e. 16 and 32bpp) */
            if (   vboxGetDisplayChangeRequest(pScrn, &cx, &cy, &cBits,
                                               &iDisplay)
                && (cBits != 16)
               )
                cBits = 24;
        }
        if (!xf86SetDepthBpp(pScrn, cBits, 0, 0, Support32bppFb))
            return FALSE;
    }
    if (pScrn->bitsPerPixel != 32 && pScrn->bitsPerPixel != 16)
    {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "The VBox additions only support 16 and 32bpp graphics modes\n");
        return FALSE;
    }
    xf86PrintDepthBpp(pScrn);

    /* options */
    xf86CollectOptions(pScrn, NULL);
    if (!(pVBox->Options = xalloc(sizeof(VBOXOptions))))
        return FALSE;
    memcpy(pVBox->Options, VBOXOptions, sizeof(VBOXOptions));
    xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, pVBox->Options);

    /* Initialise CRTC and output configuration for use with randr1.2. */
    xf86CrtcConfigInit(pScrn, &VBOXCrtcConfigFuncs);

    /* Setup our single virtual CRTC. */
    xf86CrtcCreate(pScrn, &VBOXCrtcFuncs);

    /* Set up our single virtual output. */
    output = xf86OutputCreate(pScrn, &VBOXOutputFuncs, "VBOX1");

    /* Set a sane minimum mode size and the maximum allowed by the available VRAM */
    {
#if 0
        unsigned maxSize, trySize = 512;

        do {
            maxSize = trySize;
            trySize += 128;
        } while (trySize * trySize * pScrn->bitsPerPixel / 8 < pScrn->videoRam * 1024);
#else
        unsigned maxSize = 32000;
#endif

        xf86CrtcSetSizeRange(pScrn, 64, 64, maxSize, maxSize);

        /* I don't know exactly what these are for (and they are only used in a couple
           of places in the X server code), but due to a bug in RandR 1.2 they place
           an upper limit on possible resolutions.  To add to the fun, they get set
           automatically if we don't do it ourselves. */
        pScrn->display->virtualX = maxSize;
        pScrn->display->virtualY = maxSize;
        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                   "The maximum supported resolution is currently %dx%d\n", maxSize, maxSize);
    }

    /* We are not interested in the monitor section in the configuration file. */
    xf86OutputUseScreenMonitor(output, FALSE);
    output->possible_crtcs = 1;
    output->possible_clones = 0;

    /* Now create our initial CRTC/output configuration. */
    if (!xf86InitialConfiguration(pScrn, TRUE)) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Initial CRTC configuration failed!\n");
        return (FALSE);
    }

    /* Colour weight - we always call this, since we are always in
       truecolour. */
    if (!xf86SetWeight(pScrn, rzeros, rzeros))
        return (FALSE);

    /* visual init */
    if (!xf86SetDefaultVisual(pScrn, -1))
        return (FALSE);

    xf86SetGamma(pScrn, gzeros);

    /* Set a default display resolution. */
    xf86SetDpi(pScrn, 96, 96);

    /* Framebuffer-related setup */
    pScrn->bitmapBitOrder = BITMAP_BIT_ORDER;

    return (TRUE);
}

/**
 * This function hooks into the chain that is called when framebuffer access
 * is allowed or disallowed by a call to EnableDisableFBAccess in the server.
 * In other words, it observes when the server wishes access to the
 * framebuffer to be enabled and when it should be disabled.  We need to know
 * this because we disable access ourselves during mode switches (presumably
 * the server should do this but it doesn't) and want to know whether to
 * restore it or not afterwards.
 */
static void
vboxEnableDisableFBAccess(int scrnIndex, Bool enable)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    VBOXPtr pVBox = VBOXGetRec(pScrn);

    TRACE3("enable=%s\n", enable ? "TRUE" : "FALSE");
    pVBox->accessEnabled = enable;
    pVBox->EnableDisableFBAccess(scrnIndex, enable);
    TRACE2;
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
 * End QUOTE.Initialise the initial video mode.
 */
static Bool
VBOXScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    VisualPtr visual;
    unsigned flags;

    if (pVBox->mapPhys == 0) {
#ifdef PCIACCESS
        pVBox->mapPhys = pVBox->pciInfo->regions[0].base_addr;
#else
        pVBox->mapPhys = pVBox->pciInfo->memBase[0];
#endif
/*        pVBox->mapSize = 1 << pVBox->pciInfo->size[0]; */
        /* Using the PCI information caused problems with
           non-powers-of-two sized video RAM configurations */
        pVBox->mapSize = inl(VBE_DISPI_IOPORT_DATA);
        pVBox->mapOff = 0;
    }

    if (!VBOXMapVidMem(pScrn))
        return (FALSE);

    /* save current video state */
    VBOXSaveRestore(pScrn, MODE_SAVE);
    pVBox->savedPal = VBESetGetPaletteData(pVBox->pVbe, FALSE, 0, 256,
                                           NULL, FALSE, FALSE);

    /* mi layer - reset the visual list (?)*/
    miClearVisualTypes();
    if (!xf86SetDefaultVisual(pScrn, -1))
        return (FALSE);
    if (!miSetVisualTypes(pScrn->depth, TrueColorMask,
                          pScrn->rgbBits, TrueColor))
        return (FALSE);
    if (!miSetPixmapDepths())
        return (FALSE);

    /* I checked in the sources, and XFree86 4.2 does seem to support
       this function for 32bpp. */
    if (!fbScreenInit(pScreen, pVBox->base,
                      pScrn->virtualX, pScrn->virtualY,
                      pScrn->xDpi, pScrn->yDpi,
                      pScrn->virtualX, pScrn->bitsPerPixel))
        return (FALSE);

    /* Fixup RGB ordering */
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
    miInitializeBackingStore(pScreen);
    xf86SetBackingStore(pScreen);

    /* We need to keep track of whether we are currently switched to a virtual
     * terminal to know whether a mode set operation is currently safe to do.
     */
    pVBox->vtSwitch = FALSE;
    /* Initialise DGA.  The cast is unfortunately correct - it gets cast back
       to (unsigned char *) later. */
    xf86DiDGAInit(pScreen, (unsigned long) pVBox->base);

    /* Initialise randr 1.2 mode-setting functions and set first mode. */
    if (!xf86CrtcScreenInit(pScreen)) {
        return FALSE;
    }

    if (!xf86SetDesiredModes(pScrn)) {
        return FALSE;
    }

    /* set the viewport */
    VBOXAdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);

    /* software cursor */
    miDCInitialize(pScreen, xf86GetPointerScreenFuncs());

    /* colourmap code - apparently, we need this even in Truecolour */
    if (!miCreateDefColormap(pScreen))
	return (FALSE);

    flags = CMAP_RELOAD_ON_MODE_SWITCH;

    if(!xf86HandleColormaps(pScreen, 256,
        8 /* DAC is switchable to 8 bits per primary color */,
        VBOXLoadPalette, NULL, flags))
        return (FALSE);

    /* Hook our observer function ito the chain which is called when
     * framebuffer access is enabled or disabled in the server, and
     * assume an initial state of enabled. */
    pVBox->accessEnabled = TRUE;
    pVBox->EnableDisableFBAccess = pScrn->EnableDisableFBAccess;
    pScrn->EnableDisableFBAccess = vboxEnableDisableFBAccess;

    pVBox->CloseScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = VBOXCloseScreen;
    pScreen->SaveScreen = xf86SaveScreen;

    /* We probably do want to support power management - even if we just use
       a dummy function. */
    xf86DPMSInit(pScreen, xf86DPMSSet, 0);

    /* Report any unused options (only for the first generation) */
    if (serverGeneration == 1)
        xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);

    if (vbox_device_available(pVBox) && vbox_open (pScrn, pScreen, pVBox)) {
        if (vbox_cursor_init(pScreen) != TRUE)
            xf86DrvMsg(scrnIndex, X_ERROR,
                       "Unable to start the VirtualBox mouse pointer integration with the host system.\n");
        if (vboxEnableVbva(pScrn) == TRUE)
            xf86DrvMsg(scrnIndex, X_INFO,
                      "The VBox video extensions are now enabled.\n");
        vboxEnableGraphicsCap(pVBox);
        /* Report the largest resolution that we support */
    }
    return (TRUE);
}

static Bool
VBOXEnterVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    bool rc;

    TRACE;
    pVBox->vtSwitch = FALSE;
    rc = xf86SetDesiredModes(pScrn);
    TRACE3("returning %s\n", rc ? "TRUE" : "FALSE");
    return rc;
}

static void
VBOXLeaveVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    VBOXPtr pVBox = VBOXGetRec(pScrn);

    TRACE;
    pVBox->vtSwitch = TRUE;
    VBOXSaveRestore(pScrn, MODE_RESTORE);
    if (vbox_device_available(pVBox))
    {
        if (pVBox->useVbva == TRUE)
            vboxDisableVbva(pScrn);
        vboxDisableGraphicsCap(pVBox);
    }
    TRACE2;
}

static Bool
VBOXCloseScreen(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    VBOXPtr pVBox = VBOXGetRec(pScrn);

    if (vbox_device_available(pVBox))
    {
        if (TRUE == pVBox->useVbva)
            vboxDisableVbva(pScrn);
        vboxDisableGraphicsCap(pVBox);
    }
    if (pScrn->vtSema) {
	VBOXSaveRestore(xf86Screens[scrnIndex], MODE_RESTORE);
	if (pVBox->savedPal)
	    VBESetGetPaletteData(pVBox->pVbe, TRUE, 0, 256,
				 pVBox->savedPal, FALSE, TRUE);
	VBOXUnmapVidMem(pScrn);
    }
    pScrn->vtSema = FALSE;

    /* Remove our observer functions from the X server call chains. */
    pScrn->EnableDisableFBAccess = pVBox->EnableDisableFBAccess;
    pScreen->CloseScreen = pVBox->CloseScreen;
    return pScreen->CloseScreen(scrnIndex, pScreen);
}

/**
 * Quoted from "How to add an (S)VGA driver to XFree86"
 * (http://www.xfree86.org/3.3.6/VGADriver.html):
 *
 * The ValidMode() function is required. It is used to check for any
 * chipset-dependent reasons why a graphics mode might not be valid. It gets
 * called by higher levels of the code after the Probe() stage. In many cases
 * no special checking will be required and this function will simply return
 * TRUE always.
 *
 * Note: we check here that our generated video modes fulfil the X server's
 * criteria for the monitor, since this can otherwise cause problems in
 * randr 1.2.
 */
static ModeStatus
VBOXValidMode(int scrn, DisplayModePtr p, Bool flag, int pass)
{
    static int warned = 0;
    ScrnInfoPtr pScrn = xf86Screens[scrn];
    MonPtr mon = pScrn->monitor;
    ModeStatus ret = MODE_BAD;
    DisplayModePtr mode;
    float v;

    TRACE3("HDisplay=%d, VDisplay=%d, flag=%s, pass=%d\n",
           p->HDisplay, p->VDisplay, flag ? "TRUE" : "FALSE", pass);
    if (pass != MODECHECK_FINAL) {
        if (!warned) {
            xf86DrvMsg(scrn, X_WARNING, "VBOXValidMode called unexpectedly\n");
            warned = 1;
        }
    }
#if 0
    /*
     * First off, if this isn't a mode we handed to the server (ie,
     * M_T_BUILTIN), then we reject it out of hand.
     */
    if (!(p->type & M_T_BUILTIN))
        return MODE_NOMODE;
#endif
    /*
     * Finally, walk through the vsync rates 1Hz at a time looking for a mode
     * that will fit.  This is assuredly a terrible way to do this, but
     * there's no obvious method for computing a mode of a given size that
     * will pass xf86CheckModeForMonitor.
     */
    for (v = mon->vrefresh[0].lo; v <= mon->vrefresh[0].hi; v++) {
        mode = xf86CVTMode(p->HDisplay, p->VDisplay, v, 0, 0);
        ret = xf86CheckModeForMonitor(mode, mon);
        xfree(mode);
        if (ret == MODE_OK)
            break;
    }

    if (ret != MODE_OK)
    {
        xf86DrvMsg(scrn, X_WARNING, "Graphics mode %s rejected by the X server\n", p->name);
    }
    TRACE3("returning %d\n", ret);
    return ret;
}

static Bool
VBOXSwitchMode(int scrnIndex, DisplayModePtr pMode, int flags)
{
    ScrnInfoPtr pScrn;
    VBOXPtr pVBox;
    Bool rc;

    TRACE3("HDisplay=%d, VDisplay=%d\n", pMode->HDisplay, pMode->VDisplay);
    pScrn = xf86Screens[scrnIndex];  /* Why does X have three ways of refering to the screen? */
    pVBox = VBOXGetRec(pScrn);
    /* We want to disable access to the framebuffer before switching mode.
     * After doing the switch, we allow access if it was allowed before. */
    if (pVBox->accessEnabled)
        pVBox->EnableDisableFBAccess(scrnIndex, FALSE);
    rc = xf86SetSingleMode(pScrn, pMode, 0);
    if (pVBox->accessEnabled)
        pVBox->EnableDisableFBAccess(scrnIndex, TRUE);
    TRACE3("returning %s\n", rc ? "TRUE" : "FALSE");
    return rc;
}

/* Set a graphics mode.  Poke the required values into registers, enable
   guest-host acceleration functions and tell the host we support advanced
   graphics functions. */
static Bool
VBOXSetMode(ScrnInfoPtr pScrn, DisplayModePtr pMode)
{
    VBOXPtr pVBox;
    Bool rc = TRUE;

    int bpp = pScrn->depth == 24 ? 32 : 16;
    TRACE3("HDisplay=%d, VDisplay=%d\n", pMode->HDisplay, pMode->VDisplay);
    pVBox = VBOXGetRec(pScrn);
    /* Don't fiddle with the hardware if we are switched
     * to a virtual terminal. */
    if (!pVBox->vtSwitch)
    {
        if (    vbox_device_available(pVBox)
            && (TRUE == pVBox->useVbva)
            && (vboxDisableVbva(pScrn) != TRUE)
           )  /* This would be bad. */
            rc = FALSE;
        if (rc)
        {
            /* Disable linear framebuffer mode before making changes to the resolution. */
            outw(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_ENABLE);
            outw(VBE_DISPI_IOPORT_DATA, VBE_DISPI_DISABLED);
            /* Unlike the resolution, the depth is fixed for a given screen
               for the lifetime of the X session. */
            outw(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_BPP);
            outw(VBE_DISPI_IOPORT_DATA, bpp);
            /* HDisplay and VDisplay are actually monitor information about
               the display part of the scanlines. */
            outw(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_XRES);
            outw(VBE_DISPI_IOPORT_DATA, pMode->HDisplay);
            outw(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_YRES);
            outw(VBE_DISPI_IOPORT_DATA, pMode->VDisplay);
            /* Set the virtual resolution.  We are still using VESA to control
               the virtual offset. */
            outw(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_VIRT_WIDTH);
            outw(VBE_DISPI_IOPORT_DATA, pScrn->displayWidth);
            /* Enable linear framebuffer mode. */
            outw(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_ENABLE);
            outw(VBE_DISPI_IOPORT_DATA,
                 VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
            /* Enable acceleration and tell the host we support graphics */
            if (vbox_device_available(pVBox))
            {
                if ((TRUE == pVBox->useVbva) && (vboxEnableVbva(pScrn) != TRUE))
                /* Bad but not fatal */
                    pVBox->useVbva = FALSE;
                vboxEnableGraphicsCap(pVBox);
            }
        }
    }
    TRACE3("returning %s\n", rc ? "TRUE" : "FALSE");
    return rc;
}

static void
VBOXAdjustFrame(int scrnIndex, int x, int y, int flags)
{
    VBOXPtr pVBox = VBOXGetRec(xf86Screens[scrnIndex]);

    TRACE;
    /* Don't fiddle with the hardware if we are switched
     * to a virtual terminal. */
    if (!pVBox->vtSwitch)
        VBESetDisplayStart(pVBox->pVbe, x, y, TRUE);
    TRACE2;
}

static void
VBOXFreeScreen(int scrnIndex, int flags)
{
    VBOXFreeRec(xf86Screens[scrnIndex]);
}

static Bool
VBOXMapVidMem(ScrnInfoPtr pScrn)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    Bool rc = TRUE;

    TRACE;
    if (NULL == pVBox->base)
    {
        pScrn->memPhysBase = pVBox->mapPhys;
        pScrn->fbOffset = pVBox->mapOff;

#ifdef PCIACCESS
        (void) pci_device_map_range(pVBox->pciInfo,
                                    pScrn->memPhysBase,
                                    pVBox->mapSize,
                                    PCI_DEV_MAP_FLAG_WRITABLE,
                                    & pVBox->base);

        if (pVBox->base) {
            pScrn->memPhysBase = pVBox->mapPhys;
            pVBox->VGAbase = xf86MapDomainMemory(pScrn->scrnIndex, 0,
                                                 pVBox->pciInfo,
                                                 0xa0000, 0x10000);
        }
#else
        pVBox->base = xf86MapPciMem(pScrn->scrnIndex,
                                    VIDMEM_FRAMEBUFFER,
                                    pVBox->pciTag, pVBox->mapPhys,
                                    (unsigned) pVBox->mapSize);

        if (pVBox->base) {
            pScrn->memPhysBase = pVBox->mapPhys;
            pVBox->VGAbase = xf86MapDomainMemory(pScrn->scrnIndex, 0,
                                                 pVBox->pciTag,
                                                 0xa0000, 0x10000);
        }
#endif
        /* We need this for saving/restoring textmode */
        pVBox->ioBase = pScrn->domainIOBase;

        rc = pVBox->base != NULL;
    }
    TRACE3("returning %s\n", rc ? "TRUE" : "FALSE");
    return rc;
}

static void
VBOXUnmapVidMem(ScrnInfoPtr pScrn)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);

    TRACE;
    if (pVBox->base == NULL)
        return;

#ifdef PCIACCESS
    (void) pci_device_unmap_range(pVBox->pciInfo,
                                  pVBox->base,
                                  pVBox->mapSize);
    xf86UnMapVidMem(pScrn->scrnIndex, pVBox->VGAbase, 0x10000);
#else
    xf86UnMapVidMem(pScrn->scrnIndex, pVBox->base,
                    (unsigned) pVBox->mapSize);
    xf86UnMapVidMem(pScrn->scrnIndex, pVBox->VGAbase, 0x10000);
#endif
    pVBox->base = NULL;
    TRACE2;
}

static void
VBOXLoadPalette(ScrnInfoPtr pScrn, int numColors, int *indices,
		LOCO *colors, VisualPtr pVisual)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    int i, idx;
#define VBOXDACDelay()							    \
    do {								    \
	   (void)inb(pVBox->ioBase + VGA_IOBASE_COLOR + VGA_IN_STAT_1_OFFSET); \
	   (void)inb(pVBox->ioBase + VGA_IOBASE_COLOR + VGA_IN_STAT_1_OFFSET); \
    } while (0)

    TRACE;
    for (i = 0; i < numColors; i++) {
	   idx = indices[i];
	   outb(pVBox->ioBase + VGA_DAC_WRITE_ADDR, idx);
	   VBOXDACDelay();
	   outb(pVBox->ioBase + VGA_DAC_DATA, colors[idx].red);
	   VBOXDACDelay();
	   outb(pVBox->ioBase + VGA_DAC_DATA, colors[idx].green);
	   VBOXDACDelay();
	   outb(pVBox->ioBase + VGA_DAC_DATA, colors[idx].blue);
	   VBOXDACDelay();
    }
    TRACE2;
}

/*
 * Just adapted from the std* functions in vgaHW.c
 */
static void
WriteAttr(VBOXPtr pVBox, int index, int value)
{
    (void) inb(pVBox->ioBase + VGA_IOBASE_COLOR + VGA_IN_STAT_1_OFFSET);

    index |= 0x20;
    outb(pVBox->ioBase + VGA_ATTR_INDEX, index);
    outb(pVBox->ioBase + VGA_ATTR_DATA_W, value);
}

static int
ReadAttr(VBOXPtr pVBox, int index)
{
    (void) inb(pVBox->ioBase + VGA_IOBASE_COLOR + VGA_IN_STAT_1_OFFSET);

    index |= 0x20;
    outb(pVBox->ioBase + VGA_ATTR_INDEX, index);
    return (inb(pVBox->ioBase + VGA_ATTR_DATA_R));
}

#define WriteMiscOut(value)	outb(pVBox->ioBase + VGA_MISC_OUT_W, value)
#define ReadMiscOut()		inb(pVBox->ioBase + VGA_MISC_OUT_R)
#define WriteSeq(index, value) \
        outb(pVBox->ioBase + VGA_SEQ_INDEX, (index));\
        outb(pVBox->ioBase + VGA_SEQ_DATA, value)

static int
ReadSeq(VBOXPtr pVBox, int index)
{
    outb(pVBox->ioBase + VGA_SEQ_INDEX, index);

    return (inb(pVBox->ioBase + VGA_SEQ_DATA));
}

#define WriteGr(index, value)				\
    outb(pVBox->ioBase + VGA_GRAPH_INDEX, index);	\
    outb(pVBox->ioBase + VGA_GRAPH_DATA, value)

static int
ReadGr(VBOXPtr pVBox, int index)
{
    outb(pVBox->ioBase + VGA_GRAPH_INDEX, index);

    return (inb(pVBox->ioBase + VGA_GRAPH_DATA));
}

#define WriteCrtc(index, value)						     \
    outb(pVBox->ioBase + (VGA_IOBASE_COLOR + VGA_CRTC_INDEX_OFFSET), index); \
    outb(pVBox->ioBase + (VGA_IOBASE_COLOR + VGA_CRTC_DATA_OFFSET), value)

static void
SeqReset(VBOXPtr pVBox, Bool start)
{
    if (start) {
	   WriteSeq(0x00, 0x01);		/* Synchronous Reset */
    }
    else {
	   WriteSeq(0x00, 0x03);		/* End Reset */
    }
}

static void
SaveFonts(ScrnInfoPtr pScrn)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    unsigned char miscOut, attr10, gr4, gr5, gr6, seq2, seq4, scrn;
    Bool cont = TRUE;

    TRACE;
    if (pVBox->fonts != NULL)
	cont = FALSE;

    if (cont)
    {
        /* If in graphics mode, don't save anything */
        attr10 = ReadAttr(pVBox, 0x10);
        if (attr10 & 0x01)
            cont = FALSE;
    }

    if (cont)
    {
        pVBox->fonts = xalloc(16384);

        /* save the registers that are needed here */
        miscOut = ReadMiscOut();
        gr4 = ReadGr(pVBox, 0x04);
        gr5 = ReadGr(pVBox, 0x05);
        gr6 = ReadGr(pVBox, 0x06);
        seq2 = ReadSeq(pVBox, 0x02);
        seq4 = ReadSeq(pVBox, 0x04);

        /* Force into colour mode */
        WriteMiscOut(miscOut | 0x01);

        scrn = ReadSeq(pVBox, 0x01) | 0x20;
        SeqReset(pVBox, TRUE);
        WriteSeq(0x01, scrn);
        SeqReset(pVBox, FALSE);

        WriteAttr(pVBox, 0x10, 0x01);	/* graphics mode */

        /*font1 */
        WriteSeq(0x02, 0x04);	/* write to plane 2 */
        WriteSeq(0x04, 0x06);	/* enable plane graphics */
        WriteGr(0x04, 0x02);	/* read plane 2 */
        WriteGr(0x05, 0x00);	/* write mode 0, read mode 0 */
        WriteGr(0x06, 0x05);	/* set graphics */
        slowbcopy_frombus(pVBox->VGAbase, pVBox->fonts, 8192);

        /* font2 */
        WriteSeq(0x02, 0x08);	/* write to plane 3 */
        WriteSeq(0x04, 0x06);	/* enable plane graphics */
        WriteGr(0x04, 0x03);	/* read plane 3 */
        WriteGr(0x05, 0x00);	/* write mode 0, read mode 0 */
        WriteGr(0x06, 0x05);	/* set graphics */
        slowbcopy_frombus(pVBox->VGAbase, pVBox->fonts + 8192, 8192);

        scrn = ReadSeq(pVBox, 0x01) & ~0x20;
        SeqReset(pVBox, TRUE);
        WriteSeq(0x01, scrn);
        SeqReset(pVBox, FALSE);

        /* Restore clobbered registers */
        WriteAttr(pVBox, 0x10, attr10);
        WriteSeq(0x02, seq2);
        WriteSeq(0x04, seq4);
        WriteGr(0x04, gr4);
        WriteGr(0x05, gr5);
        WriteGr(0x06, gr6);
        WriteMiscOut(miscOut);
    }
    TRACE2;
}

static void
RestoreFonts(ScrnInfoPtr pScrn)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    unsigned char miscOut, attr10, gr1, gr3, gr4, gr5, gr6, gr8, seq2, seq4, scrn;

    TRACE;
    if (pVBox->fonts != NULL)
    {
        /* save the registers that are needed here */
        miscOut = ReadMiscOut();
        attr10 = ReadAttr(pVBox, 0x10);
        gr1 = ReadGr(pVBox, 0x01);
        gr3 = ReadGr(pVBox, 0x03);
        gr4 = ReadGr(pVBox, 0x04);
        gr5 = ReadGr(pVBox, 0x05);
        gr6 = ReadGr(pVBox, 0x06);
        gr8 = ReadGr(pVBox, 0x08);
        seq2 = ReadSeq(pVBox, 0x02);
        seq4 = ReadSeq(pVBox, 0x04);

        /* Force into colour mode */
        WriteMiscOut(miscOut | 0x01);

        scrn = ReadSeq(pVBox, 0x01) & ~0x20;
        SeqReset(pVBox, TRUE);
        WriteSeq(0x01, scrn);
        SeqReset(pVBox, FALSE);

        WriteAttr(pVBox, 0x10, 0x01);	/* graphics mode */
        if (pScrn->depth == 4) {
	    /* GJA */
	    WriteGr(0x03, 0x00);	/* don't rotate, write unmodified */
	    WriteGr(0x08, 0xFF);	/* write all bits in a byte */
	    WriteGr(0x01, 0x00);	/* all planes come from CPU */
        }

        WriteSeq(0x02, 0x04);   /* write to plane 2 */
        WriteSeq(0x04, 0x06);   /* enable plane graphics */
        WriteGr(0x04, 0x02);    /* read plane 2 */
        WriteGr(0x05, 0x00);    /* write mode 0, read mode 0 */
        WriteGr(0x06, 0x05);    /* set graphics */
        slowbcopy_tobus(pVBox->fonts, pVBox->VGAbase, 8192);

        WriteSeq(0x02, 0x08);   /* write to plane 3 */
        WriteSeq(0x04, 0x06);   /* enable plane graphics */
        WriteGr(0x04, 0x03);    /* read plane 3 */
        WriteGr(0x05, 0x00);    /* write mode 0, read mode 0 */
        WriteGr(0x06, 0x05);    /* set graphics */
        slowbcopy_tobus(pVBox->fonts + 8192, pVBox->VGAbase, 8192);

        scrn = ReadSeq(pVBox, 0x01) & ~0x20;
        SeqReset(pVBox, TRUE);
        WriteSeq(0x01, scrn);
        SeqReset(pVBox, FALSE);

        /* restore the registers that were changed */
        WriteMiscOut(miscOut);
        WriteAttr(pVBox, 0x10, attr10);
        WriteGr(0x01, gr1);
        WriteGr(0x03, gr3);
        WriteGr(0x04, gr4);
        WriteGr(0x05, gr5);
        WriteGr(0x06, gr6);
        WriteGr(0x08, gr8);
        WriteSeq(0x02, seq2);
        WriteSeq(0x04, seq4);
    }
    TRACE2;
}

Bool
VBOXSaveRestore(ScrnInfoPtr pScrn, vbeSaveRestoreFunction function)
{
    VBOXPtr pVBox;
    Bool rc = TRUE;

    TRACE;
    if (MODE_QUERY < 0 || function > MODE_RESTORE)
	rc = FALSE;

    if (rc)
    {
        pVBox = VBOXGetRec(pScrn);

        /* Query amount of memory to save state */
        if (function == MODE_QUERY ||
    	    (function == MODE_SAVE && pVBox->state == NULL))
        {

	    /* Make sure we save at least this information in case of failure */
            (void)VBEGetVBEMode(pVBox->pVbe, &pVBox->stateMode);
            SaveFonts(pScrn);

            if (!VBESaveRestore(pVBox->pVbe,function,(pointer)&pVBox->state,
                                &pVBox->stateSize,&pVBox->statePage)
               )
                rc = FALSE;
        }
    }
    if (rc)
    {
        /* Save/Restore Super VGA state */
        if (function != MODE_QUERY) {

            if (function == MODE_RESTORE)
                memcpy(pVBox->state, pVBox->pstate,
                       (unsigned) pVBox->stateSize);

            if (   (rc = VBESaveRestore(pVBox->pVbe,function,
                                        (pointer)&pVBox->state,
                                        &pVBox->stateSize,&pVBox->statePage)
                   )
                && (function == MODE_SAVE)
               )
            {
                /* don't rely on the memory not being touched */
                if (pVBox->pstate == NULL)
                    pVBox->pstate = xalloc(pVBox->stateSize);
                memcpy(pVBox->pstate, pVBox->state,
                       (unsigned) pVBox->stateSize);
            }

            if (function == MODE_RESTORE)
            {
                VBESetVBEMode(pVBox->pVbe, pVBox->stateMode, NULL);
                RestoreFonts(pScrn);
            }
        }
    }
    TRACE3("returning %s\n", rc ? "TRUE" : "FALSE");
    return rc;
}
