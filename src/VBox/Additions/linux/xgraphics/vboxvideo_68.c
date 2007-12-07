/** @file
 *
 * Linux Additions X11 graphics driver
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

#define DEBUG_VERB 2

#include "vboxvideo.h"
#include "version-generated.h"

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

/* Mandatory functions */

static const OptionInfoRec * VBOXAvailableOptions(int chipid, int busid);
static void VBOXIdentify(int flags);
static Bool VBOXProbe(DriverPtr drv, int flags);
static Bool VBOXPreInit(ScrnInfoPtr pScrn, int flags);
static Bool VBOXScreenInit(int Index, ScreenPtr pScreen, int argc,
                           char **argv);
static Bool VBOXEnterVT(int scrnIndex, int flags);
static void VBOXLeaveVT(int scrnIndex, int flags);
static Bool VBOXCloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool VBOXSaveScreen(ScreenPtr pScreen, int mode);
static Bool VBOXSwitchMode(int scrnIndex, DisplayModePtr pMode, int flags);
static Bool VBOXSetMode(ScrnInfoPtr pScrn, DisplayModePtr pMode);
static void VBOXAdjustFrame(int scrnIndex, int x, int y, int flags);
static void VBOXFreeScreen(int scrnIndex, int flags);
static void VBOXFreeRec(ScrnInfoPtr pScrn);
static void VBOXDisplayPowerManagementSet(ScrnInfoPtr pScrn, int mode,
                                          int flags);

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

/* Initialise DGA */

static Bool VBOXDGAInit(ScrnInfoPtr pScrn, ScreenPtr pScreen);

/*
 * This contains the functions needed by the server after loading the
 * driver module.  It must be supplied, and gets added the driver list by
 * the Module Setup funtion in the dynamic case.  In the static case a
 * reference to this is compiled in, and this requires that the name of
 * this DriverRec be an upper-case version of the driver name.
 */

DriverRec VBOXDRV = {
    VBOX_VERSION,
    VBOX_DRIVER_NAME,
    VBOXIdentify,
    VBOXProbe,
    VBOXAvailableOptions,
    NULL,
    0
};

/* Supported chipsets */
static SymTabRec VBOXChipsets[] =
{
    {VBOX_VESA_DEVICEID, "vbox"},
    {-1,	 NULL}
};

static PciChipsets VBOXPCIchipsets[] = {
  { VBOX_DEVICEID, VBOX_DEVICEID, RES_SHARED_VGA },
  { -1,		-1,		    RES_UNDEFINED },
};

typedef enum {
    OPTION_SHADOW_FB
} VBOXOpts;

/* No options for now */
static const OptionInfoRec VBOXOptions[] = {
    { -1,		NULL,		OPTV_NONE,	{0},	FALSE }
};

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

static const char *vbeSymbols[] = {
    "VBEFreeModeInfo",
    "VBEGetModeInfo",
    "VBEGetVBEInfo",
    "VBEGetVBEMode",
    "VBEInit",
    "VBESaveRestore",
    "VBESetDisplayStart",
    "VBESetGetDACPaletteFormat",
    "VBESetGetLogicalScanlineLength",
    "VBESetGetPaletteData",
    "VBESetVBEMode",
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
    "innotek GmbH",
    MODINFOSTRING1,
    MODINFOSTRING2,
    XF86_VERSION_CURRENT,
    1,                          /* Module major version. Xorg-specific */
    0,                          /* Module minor version. Xorg-specific */
    0,                          /* Module patchlevel. Xorg-specific */
    ABI_CLASS_VIDEODRV,	        /* This is a video driver */
    ABI_VIDEODRV_VERSION,
    MOD_CLASS_VIDEODRV,
    {0, 0, 0, 0}
};

/*
 * This data is accessed by the loader.  The name must be the module name
 * followed by "ModuleData".
 */
XF86ModuleData vboxvideoModuleData = { &vboxVersionRec, vboxSetup, NULL };

static pointer
vboxSetup(pointer Module, pointer Options, int *ErrorMajor, int *ErrorMinor)
{
    static Bool Initialised = FALSE;

    if (!Initialised)
    {
        Initialised = TRUE;
        xf86AddDriver(&VBOXDRV, Module, 0);
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

static Bool
VBOXProbe(DriverPtr drv, int flags)
{
    Bool foundScreen = FALSE;
    int numDevSections, numUsed;
    GDevPtr *devSections;
    int *usedChips;
    int i;

    /*
     * Find the config file Device sections that match this
     * driver, and return if there are none.
     */
    if ((numDevSections = xf86MatchDevice(VBOX_NAME,
					  &devSections)) <= 0)
	return (FALSE);

    /* PCI BUS */
    if (xf86GetPciVideoInfo()) {
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

static VBOXPtr
VBOXGetRec(ScrnInfoPtr pScrn)
{
    if (!pScrn->driverPrivate)
    {
        pScrn->driverPrivate = xcalloc(sizeof(VBOXRec), 1);
        ((VBOXPtr)pScrn->driverPrivate)->vbox_fd = -1;
    }

    return ((VBOXPtr)pScrn->driverPrivate);
}

static void
VBOXFreeRec(ScrnInfoPtr pScrn)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);
#if 0
    xfree(pVBox->vbeInfo);
#endif
    xfree(pVBox->savedPal);
    xfree(pVBox->fonts);
    xfree(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;
}

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
    ClockRange *clockRanges;
    int i;
    DisplayModePtr m_prev;

    /* Are we really starting the server, or is this just a dummy run? */
    if (flags & PROBE_DETECT)
        return (FALSE);

    xf86Msg(X_INFO,
            "VirtualBox guest additions video driver version "
            VBOX_VERSION_STRING "\n");

    /* Get our private data from the ScrnInfoRec structure. */
    pVBox = VBOXGetRec(pScrn);

    /* Entity information seems to mean bus information. */
    pVBox->pEnt = xf86GetEntityInfo(pScrn->entityList[0]);
    if (pVBox->pEnt->location.type != BUS_PCI)
        return FALSE;

    /* The ramdac module is needed for the hardware cursor. */
    if (!xf86LoadSubModule(pScrn, "ramdac"))
        return FALSE;
    xf86LoaderReqSymLists(ramdacSymbols, NULL);

    /* We need the vbe module because we use VBE code to save and restore
       text mode, in order to keep our code simple. */
    if (!xf86LoadSubModule(pScrn, "vbe"))
        return (FALSE);
    xf86LoaderReqSymLists(vbeSymbols, NULL);

    /* The framebuffer module. */
    if (xf86LoadSubModule(pScrn, "fb") == NULL)
        return (FALSE);
    xf86LoaderReqSymLists(fbSymbols, NULL);

    if (!xf86LoadSubModule(pScrn, "shadowfb"))
        return FALSE;
    xf86LoaderReqSymLists(shadowfbSymbols, NULL);

    pVBox->pciInfo = xf86GetPciInfoForEntity(pVBox->pEnt->index);
    pVBox->pciTag = pciTag(pVBox->pciInfo->bus,
                           pVBox->pciInfo->device,
                           pVBox->pciInfo->func);

    /* Set up our ScrnInfoRec structure to describe our virtual
       capabilities to X. */

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

    pScrn->chipset = "vbox";
    pScrn->progClock = TRUE;

    /* Determine the size of the VBox video RAM from PCI data*/
#if 0
    pScrn->videoRam = 1 << pVBox->pciInfo->size[0];
#endif
    /* Using the PCI information caused problems with non-powers-of-two
       sized video RAM configurations */
    pScrn->videoRam = inl(VBE_DISPI_IOPORT_DATA) / 1024;

    /* Set up clock information that will support all modes we need. */
    clockRanges = xnfcalloc(sizeof(ClockRange), 1);
    clockRanges->next = NULL;
    clockRanges->minClock = 1000;
    clockRanges->maxClock = 1000000000;
    clockRanges->clockIndex = -1;
    clockRanges->ClockMulFactor = 1;
    clockRanges->ClockDivFactor = 1;

    /* This function asks X to choose a depth and bpp based on the
       config file and the command line, and gives a default in
       case none is specified.  Note that we only support 32bpp, not
       24bpp.  After spending ages looking through the XFree86 4.2
       source code however, I realised that it automatically uses
       32bpp for depth 24 unless you explicitly add a "24 24"
       format to its internal list. */
    if (!xf86SetDepthBpp(pScrn, pScrn->videoRam >= 2048 ? 24 : 16, 0, 0,
                         Support32bppFb))
        return FALSE;
    if (pScrn->depth != 24 && pScrn->depth != 16)
    {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "The VBox additions only support 16 and 32bpp graphics modes\n");
        return FALSE;
    }
    xf86PrintDepthBpp(pScrn);

    /* Colour weight - we always call this, since we are always in
       truecolour. */
    if (!xf86SetWeight(pScrn, rzeros, rzeros))
        return (FALSE);

    /* visual init */
    if (!xf86SetDefaultVisual(pScrn, -1))
        return (FALSE);

    xf86SetGamma(pScrn, gzeros);

    /* To get around the problem of SUSE specifying a single, invalid mode in their
     * Xorg.conf by default, we add an additional mode to the end of the user specified
     * list. This means that if all user modes are invalid, X will try our mode before
     * falling back to its standard mode list. */
    if (pScrn->display->modes == NULL)
    {
        /* The user specified no modes at all - specify 1024x768 as a default. */
        pScrn->display->modes    = xnfalloc(4 * sizeof(char*));
        pScrn->display->modes[0] = "1024x768";
        pScrn->display->modes[1] = "800x600";
        pScrn->display->modes[2] = "640x480";
        pScrn->display->modes[3] = NULL;
    }
    else
    {
        /* Add 1024x768 to the end of the mode list in case the others are all invalid. */
        for (i = 0; pScrn->display->modes[i] != NULL; i++);
        pScrn->display->modes      = xnfrealloc(pScrn->display->modes, (i + 4)
                                   * sizeof(char *));
        pScrn->display->modes[i  ] = "1024x768";
        pScrn->display->modes[i+1] = "800x600";
        pScrn->display->modes[i+2] = "640x480";
        pScrn->display->modes[i+3] = NULL;
    }

    /* Determine the virtual screen resolution from the first mode (which will be selected) */
    sscanf(pScrn->display->modes[0], "%dx%d",
           &pScrn->display->virtualX, &pScrn->display->virtualY);
    pScrn->display->virtualX = (pScrn->display->virtualX + 7) & ~7;

    /* Create a builtin mode for every specified mode. This allows to specify arbitrary
     * screen resolutions */
    m_prev = NULL;
    for (i = 0; pScrn->display->modes[i] != NULL; i++)
    {
        DisplayModePtr m;
        int x = 0, y = 0;

        sscanf(pScrn->display->modes[i], "%dx%d", &x, &y);
        /* sanity check, smaller resolutions does not make sense */
        if (x < 64 || y < 64)
        {
            xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Ignoring mode \"%s\"\n",
                       pScrn->display->modes[i]);
            continue;
        }
        m                = xnfcalloc(sizeof(DisplayModeRec), 1);
        m->status        = MODE_OK;
        m->type          = M_T_BUILTIN;
        /* VBox does only support screen widths which are a multiple of 8 */
        m->HDisplay      = (x + 7) & ~7;
        m->VDisplay      = y;
        m->name          = strdup(pScrn->display->modes[i]);
        if (!m_prev)
            pScrn->modePool = m;
        else
            m_prev->next = m;
        m->prev = m_prev;
        m_prev  = m;
    }

    /* Filter out video modes not supported by the virtual hardware
       we described.  All modes used by the Windows additions should
       work fine. */
    i = xf86ValidateModes(pScrn, pScrn->monitor->Modes,
                          pScrn->display->modes,
                          clockRanges, NULL, 0, 6400, 1, 0, 1440,
                          pScrn->display->virtualX,
                          pScrn->display->virtualY,
                          pScrn->videoRam, LOOKUP_BEST_REFRESH);

    if (i <= 0) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No usable graphics modes found.\n");
        return (FALSE);
    }
    xf86PruneDriverModes(pScrn);

    pScrn->currentMode = pScrn->modes;
    pScrn->displayWidth = pScrn->virtualX;

    xf86PrintModes(pScrn);

    /* Set display resolution.  This was arbitrarily chosen to be about the same as my monitor. */
    xf86SetDpi(pScrn, 100, 100);

    if (pScrn->modes == NULL) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No graphics modes available\n");
        return (FALSE);
    }

    /* options */
    xf86CollectOptions(pScrn, NULL);
    if (!(pVBox->Options = xalloc(sizeof(VBOXOptions))))
        return FALSE;
    memcpy(pVBox->Options, VBOXOptions, sizeof(VBOXOptions));
    xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, pVBox->Options);

    /* Framebuffer-related setup */
    pScrn->bitmapBitOrder = BITMAP_BIT_ORDER;
    return (TRUE);
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

    /* We make use of the X11 VBE code to save and restore text mode, in
       order to keep our code simple. */
    if ((pVBox->pVbe = VBEInit(NULL, pVBox->pEnt->index)) == NULL)
        return (FALSE);

    if (pVBox->mapPhys == 0) {
        pVBox->mapPhys = pVBox->pciInfo->memBase[0];
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

    /* set first video mode */
    if (!VBOXSetMode(pScrn, pScrn->currentMode))
        return FALSE;

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
                      pScrn->displayWidth, pScrn->bitsPerPixel))
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

    VBOXDGAInit(pScrn, pScreen);

    xf86SetBlackWhitePixels(pScreen);
    miInitializeBackingStore(pScreen);
    xf86SetBackingStore(pScreen);

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

    pVBox->CloseScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = VBOXCloseScreen;
    pScreen->SaveScreen = VBOXSaveScreen;

    /* However, we probably do want to support power management -
       even if we just use a dummy function. */
    xf86DPMSInit(pScreen, VBOXDisplayPowerManagementSet, 0);

    /* Report any unused options (only for the first generation) */
    if (serverGeneration == 1)
        xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);

    if (vbox_open (pScrn, pScreen, pVBox)) {
        if (vbox_cursor_init(pScreen) != TRUE)
            xf86DrvMsg(scrnIndex, X_ERROR,
                       "Unable to start the VirtualBox mouse pointer integration with the host system.\n");
        if (vboxEnableVbva(pScrn) == TRUE)
            xf86DrvMsg(scrnIndex, X_INFO,
                      "The VBox video extensions are now enabled.\n");
    } else
        xf86DrvMsg(scrnIndex, X_ERROR, "Failed to open the VBox system device - make sure that the VirtualBox guest additions are properly installed.  If you are not sure, try reinstalling them.\n");
    return (TRUE);
}

static Bool
VBOXEnterVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    VBOXPtr pVBox = VBOXGetRec(pScrn);

    if (!VBOXSetMode(pScrn, pScrn->currentMode))
        return FALSE;
    if (pVBox->useVbva == TRUE)
        vboxEnableVbva(pScrn);
    return TRUE;
}

static void
VBOXLeaveVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    VBOXPtr pVBox = VBOXGetRec(pScrn);

    VBOXSaveRestore(pScrn, MODE_RESTORE);
    if (pVBox->useVbva == TRUE)
        vboxDisableVbva(pScrn);
}

static Bool
VBOXCloseScreen(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    VBOXPtr pVBox = VBOXGetRec(pScrn);

    if (pVBox->useVbva == TRUE)
        vboxDisableVbva(pScrn);
    if (pScrn->vtSema) {
	VBOXSaveRestore(xf86Screens[scrnIndex], MODE_RESTORE);
	VBESetGetPaletteData(pVBox->pVbe, TRUE, 0, 256,
			     pVBox->savedPal, FALSE, TRUE);
	VBOXUnmapVidMem(pScrn);
    }
    if (pVBox->pDGAMode) {
	xfree(pVBox->pDGAMode);
	pVBox->pDGAMode = NULL;
	pVBox->nDGAMode = 0;
    }
    pScrn->vtSema = FALSE;

    pScreen->CloseScreen = pVBox->CloseScreen;
    return pScreen->CloseScreen(scrnIndex, pScreen);
}

static Bool
VBOXSwitchMode(int scrnIndex, DisplayModePtr pMode, int flags)
{
    ScrnInfoPtr pScrn;
    VBOXPtr pVBox;

    pScrn = xf86Screens[scrnIndex];  /* Why does X have three ways of refering to the screen? */
    pVBox = VBOXGetRec(pScrn);
    if (pVBox->useVbva == TRUE)
        if (vboxDisableVbva(pScrn) != TRUE)  /* This would be bad. */
            return FALSE;
    if (VBOXSetMode(pScrn, pMode) != TRUE)
        return FALSE;
    if (pVBox->useVbva == TRUE)
        if (vboxEnableVbva(pScrn) != TRUE)  /* Bad but not fatal */
            pVBox->useVbva = FALSE;
    return TRUE;
}

/* Set a graphics mode */
static Bool
VBOXSetMode(ScrnInfoPtr pScrn, DisplayModePtr pMode)
{
    VBOXPtr pVBox;

    int bpp = pScrn->depth == 24 ? 32 : 16;
    int xRes = pMode->HDisplay;
    if (pScrn->virtualX * pScrn->virtualY * bpp / 8
        >= pScrn->videoRam * 1024)
    {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "Unable to set up a virtual screen size of %dx%d with %d Kb of video memory.  Please increase the video memory size.\n",
                   pScrn->virtualX, pScrn->virtualY, pScrn->videoRam);
        return FALSE;
    }
    /* We only support horizontal resolutions which are a multiple of 8.  Round down if
       necessary. */
    if (xRes % 8 != 0)
    {
        xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                   "VirtualBox only supports screen widths which are a multiple of 8.  Rounding down from %d to %d\n",
                   xRes, xRes - (xRes % 8));
        xRes = xRes - (xRes % 8);
    }
    pVBox = VBOXGetRec(pScrn);
    pScrn->vtSema = TRUE;
    /* Disable linear framebuffer mode before making changes to the resolution. */
    outw(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_ENABLE);
    outw(VBE_DISPI_IOPORT_DATA,
         VBE_DISPI_DISABLED);
    /* Unlike the resolution, the depth is fixed for a given screen
       for the lifetime of the X session. */
    outw(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_BPP);
    outw(VBE_DISPI_IOPORT_DATA, bpp);
    /* HDisplay and VDisplay are actually monitor information about
       the display part of the scanlines. */
    outw(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_XRES);
    outw(VBE_DISPI_IOPORT_DATA, xRes);
    outw(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_YRES);
    outw(VBE_DISPI_IOPORT_DATA, pMode->VDisplay);
    /* Enable linear framebuffer mode. */
    outw(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_ENABLE);
    outw(VBE_DISPI_IOPORT_DATA,
         VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
    /* Set the virtual resolution.  We are still using VESA to control
       the virtual offset. */
    outw(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_VIRT_WIDTH);
    outw(VBE_DISPI_IOPORT_DATA, pScrn->displayWidth);
    return (TRUE);
}

static void
VBOXAdjustFrame(int scrnIndex, int x, int y, int flags)
{
    VBOXPtr pVBox = VBOXGetRec(xf86Screens[scrnIndex]);

    VBESetDisplayStart(pVBox->pVbe, x, y, TRUE);
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

    if (pVBox->base != NULL)
        return (TRUE);

    pScrn->memPhysBase = pVBox->mapPhys;
    pScrn->fbOffset = pVBox->mapOff;

    pVBox->base = xf86MapPciMem(pScrn->scrnIndex,
                                VIDMEM_FRAMEBUFFER,
                                pVBox->pciTag, pVBox->mapPhys,
                                (unsigned) pVBox->mapSize);

    if (pVBox->base) {
        pScrn->memPhysBase = pVBox->mapPhys;
        pVBox->VGAbase = xf86MapVidMem(pScrn->scrnIndex, 0,
                                       0xa0000, 0x10000);
    }

    return (pVBox->base != NULL);
}

static void
VBOXUnmapVidMem(ScrnInfoPtr pScrn)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);

    if (pVBox->base == NULL)
        return;

    xf86UnMapVidMem(pScrn->scrnIndex, pVBox->base,
                    (unsigned) pVBox->mapSize);
    xf86UnMapVidMem(pScrn->scrnIndex, pVBox->VGAbase, 0x10000);
    pVBox->base = NULL;
}

static void
VBOXLoadPalette(ScrnInfoPtr pScrn, int numColors, int *indices,
		LOCO *colors, VisualPtr pVisual)
{
#define WriteDacWriteAddr(value)	outb(VGA_DAC_WRITE_ADDR, value)
#define WriteDacData(value)		outb(VGA_DAC_DATA, value);
#undef DACDelay
#define DACDelay()								\
	do {									\
	    unsigned char temp = inb(VGA_IOBASE_COLOR + VGA_IN_STAT_1_OFFSET);	\
	    temp = inb(VGA_IOBASE_COLOR + VGA_IN_STAT_1_OFFSET);		\
	} while (0)
    int i, idx;

    for (i = 0; i < numColors; i++) {
	   idx = indices[i];
	   WriteDacWriteAddr(idx);
	   DACDelay();
	   WriteDacData(colors[idx].red);
	   DACDelay();
	   WriteDacData(colors[idx].green);
	   DACDelay();
	   WriteDacData(colors[idx].blue);
	   DACDelay();
    }
}

/*
 * Just adapted from the std* functions in vgaHW.c
 */
static void
WriteAttr(int index, int value)
{
    CARD8 tmp;

    tmp = inb(VGA_IOBASE_COLOR + VGA_IN_STAT_1_OFFSET);

    index |= 0x20;
    outb(VGA_ATTR_INDEX, index);
    outb(VGA_ATTR_DATA_W, value);
}

static int
ReadAttr(int index)
{
    CARD8 tmp;

    tmp = inb(VGA_IOBASE_COLOR + VGA_IN_STAT_1_OFFSET);

    index |= 0x20;
    outb(VGA_ATTR_INDEX, index);
    return (inb(VGA_ATTR_DATA_R));
}

#define WriteMiscOut(value)	outb(VGA_MISC_OUT_W, value)
#define ReadMiscOut()		inb(VGA_MISC_OUT_R)
#define WriteSeq(index, value) \
        outb(VGA_SEQ_INDEX, (index));\
        outb(VGA_SEQ_DATA, value)

static int
ReadSeq(int index)
{
    outb(VGA_SEQ_INDEX, index);

    return (inb(VGA_SEQ_DATA));
}

#define WriteGr(index, value)	outb(VGA_GRAPH_INDEX, index);\
				outb(VGA_GRAPH_DATA, value)

static int
ReadGr(int index)
{
    outb(VGA_GRAPH_INDEX, index);

    return (inb(VGA_GRAPH_DATA));
}

#define WriteCrtc(index, value)	outb(VGA_CRTC_INDEX_OFFSET, index);\
				outb(VGA_CRTC_DATA_OFFSET, value)

static void
SeqReset(Bool start)
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

    if (pVBox->fonts != NULL)
	return;

    /* If in graphics mode, don't save anything */
    attr10 = ReadAttr(0x10);
    if (attr10 & 0x01)
	return;

    pVBox->fonts = xalloc(16384);

    /* save the registers that are needed here */
    miscOut = ReadMiscOut();
    gr4 = ReadGr(0x04);
    gr5 = ReadGr(0x05);
    gr6 = ReadGr(0x06);
    seq2 = ReadSeq(0x02);
    seq4 = ReadSeq(0x04);

    /* Force into colour mode */
    WriteMiscOut(miscOut | 0x01);

    scrn = ReadSeq(0x01) | 0x20;
    SeqReset(TRUE);
    WriteSeq(0x01, scrn);
    SeqReset(FALSE);

    WriteAttr(0x10, 0x01);	/* graphics mode */

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

    scrn = ReadSeq(0x01) & ~0x20;
    SeqReset(TRUE);
    WriteSeq(0x01, scrn);
    SeqReset(FALSE);

    /* Restore clobbered registers */
    WriteAttr(0x10, attr10);
    WriteSeq(0x02, seq2);
    WriteSeq(0x04, seq4);
    WriteGr(0x04, gr4);
    WriteGr(0x05, gr5);
    WriteGr(0x06, gr6);
    WriteMiscOut(miscOut);
}

static void
RestoreFonts(ScrnInfoPtr pScrn)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    unsigned char miscOut, attr10, gr1, gr3, gr4, gr5, gr6, gr8, seq2, seq4, scrn;

    if (pVBox->fonts == NULL)
        return;

    /* save the registers that are needed here */
    miscOut = ReadMiscOut();
    attr10 = ReadAttr(0x10);
    gr1 = ReadGr(0x01);
    gr3 = ReadGr(0x03);
    gr4 = ReadGr(0x04);
    gr5 = ReadGr(0x05);
    gr6 = ReadGr(0x06);
    gr8 = ReadGr(0x08);
    seq2 = ReadSeq(0x02);
    seq4 = ReadSeq(0x04);

    /* Force into colour mode */
    WriteMiscOut(miscOut | 0x01);

    scrn = ReadSeq(0x01) | 0x20;
    SeqReset(TRUE);
    WriteSeq(0x01, scrn);
    SeqReset(FALSE);

    WriteAttr(0x10, 0x01);	/* graphics mode */
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

    scrn = ReadSeq(0x01) & ~0x20;
    SeqReset(TRUE);
    WriteSeq(0x01, scrn);
    SeqReset(FALSE);

    /* restore the registers that were changed */
    WriteMiscOut(miscOut);
    WriteAttr(0x10, attr10);
    WriteGr(0x01, gr1);
    WriteGr(0x03, gr3);
    WriteGr(0x04, gr4);
    WriteGr(0x05, gr5);
    WriteGr(0x06, gr6);
    WriteGr(0x08, gr8);
    WriteSeq(0x02, seq2);
    WriteSeq(0x04, seq4);
}

static Bool
VBOXSaveScreen(ScreenPtr pScreen, int mode)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    Bool on = xf86IsUnblank(mode);

    if (on)
	SetTimeSinceLastInputEvent();

    if (pScrn->vtSema) {
	unsigned char scrn = ReadSeq(0x01);

	if (on)
	    scrn &= ~0x20;
	else
	    scrn |= 0x20;
	SeqReset(TRUE);
	WriteSeq(0x01, scrn);
	SeqReset(FALSE);
    }

    return (TRUE);
}

Bool
VBOXSaveRestore(ScrnInfoPtr pScrn, vbeSaveRestoreFunction function)
{
    VBOXPtr pVBox;

    if (MODE_QUERY < 0 || function > MODE_RESTORE)
	return (FALSE);

    pVBox = VBOXGetRec(pScrn);


    /* Query amount of memory to save state */
    if (function == MODE_QUERY ||
	(function == MODE_SAVE && pVBox->state == NULL))
    {

	/* Make sure we save at least this information in case of failure */
	(void)VBEGetVBEMode(pVBox->pVbe, &pVBox->stateMode);
	SaveFonts(pScrn);

        if (!VBESaveRestore(pVBox->pVbe,function,(pointer)&pVBox->state,
                            &pVBox->stateSize,&pVBox->statePage))
            return FALSE;
    }

    /* Save/Restore Super VGA state */
    if (function != MODE_QUERY) {
        Bool retval = TRUE;

        if (function == MODE_RESTORE)
            memcpy(pVBox->state, pVBox->pstate,
                   (unsigned) pVBox->stateSize);

        if ((retval = VBESaveRestore(pVBox->pVbe,function,
                                     (pointer)&pVBox->state,
                                     &pVBox->stateSize,&pVBox->statePage))
             && function == MODE_SAVE)
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

	if (!retval)
	    return (FALSE);

    }

    return (TRUE);
}

static void
VBOXDisplayPowerManagementSet(ScrnInfoPtr pScrn, int mode,
                int flags)
{
    /* VBox is always power efficient... */
}




/***********************************************************************
 * DGA stuff
 ***********************************************************************/
static Bool VBOXDGAOpenFramebuffer(ScrnInfoPtr pScrn, char **DeviceName,
				   unsigned char **ApertureBase,
				   int *ApertureSize, int *ApertureOffset,
				   int *flags);
static Bool VBOXDGASetMode(ScrnInfoPtr pScrn, DGAModePtr pDGAMode);
static void VBOXDGASetViewport(ScrnInfoPtr pScrn, int x, int y, int flags);

static Bool
VBOXDGAOpenFramebuffer(ScrnInfoPtr pScrn, char **DeviceName,
		       unsigned char **ApertureBase, int *ApertureSize,
		       int *ApertureOffset, int *flags)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);

    *DeviceName = NULL;		/* No special device */
    *ApertureBase = (unsigned char *)(long)(pVBox->mapPhys);
    *ApertureSize = pVBox->mapSize;
    *ApertureOffset = pVBox->mapOff;
    *flags = DGA_NEED_ROOT;

    return (TRUE);
}

static Bool
VBOXDGASetMode(ScrnInfoPtr pScrn, DGAModePtr pDGAMode)
{
    DisplayModePtr pMode;
    int scrnIdx = pScrn->pScreen->myNum;
    int frameX0, frameY0;

    if (pDGAMode) {
	pMode = pDGAMode->mode;
	frameX0 = frameY0 = 0;
    }
    else {
	if (!(pMode = pScrn->currentMode))
	    return (TRUE);

	frameX0 = pScrn->frameX0;
	frameY0 = pScrn->frameY0;
    }

    if (!(*pScrn->SwitchMode)(scrnIdx, pMode, 0))
	return (FALSE);
    (*pScrn->AdjustFrame)(scrnIdx, frameX0, frameY0, 0);

    return (TRUE);
}

static void
VBOXDGASetViewport(ScrnInfoPtr pScrn, int x, int y, int flags)
{
    (*pScrn->AdjustFrame)(pScrn->pScreen->myNum, x, y, flags);
}

static int
VBOXDGAGetViewport(ScrnInfoPtr pScrn)
{
    return (0);
}

static DGAFunctionRec VBOXDGAFunctions =
{
    VBOXDGAOpenFramebuffer,
    NULL,       /* CloseFramebuffer */
    VBOXDGASetMode,
    VBOXDGASetViewport,
    VBOXDGAGetViewport,
    NULL,       /* Sync */
    NULL,       /* FillRect */
    NULL,       /* BlitRect */
    NULL,       /* BlitTransRect */
};

static void
VBOXDGAAddModes(ScrnInfoPtr pScrn)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    DisplayModePtr pMode = pScrn->modes;
    DGAModePtr pDGAMode;

    do {
	pDGAMode = xrealloc(pVBox->pDGAMode,
			    (pVBox->nDGAMode + 1) * sizeof(DGAModeRec));
	if (!pDGAMode)
	    break;

	pVBox->pDGAMode = pDGAMode;
	pDGAMode += pVBox->nDGAMode;
	(void)memset(pDGAMode, 0, sizeof(DGAModeRec));

	++pVBox->nDGAMode;
	pDGAMode->mode = pMode;
	pDGAMode->flags = DGA_CONCURRENT_ACCESS | DGA_PIXMAP_AVAILABLE;
	pDGAMode->byteOrder = pScrn->imageByteOrder;
	pDGAMode->depth = pScrn->depth;
	pDGAMode->bitsPerPixel = pScrn->bitsPerPixel;
	pDGAMode->red_mask = pScrn->mask.red;
	pDGAMode->green_mask = pScrn->mask.green;
	pDGAMode->blue_mask = pScrn->mask.blue;
        pDGAMode->visualClass = TrueColor;
	pDGAMode->xViewportStep = 1;
	pDGAMode->yViewportStep = 1;
	pDGAMode->viewportWidth = pMode->HDisplay;
	pDGAMode->viewportHeight = pMode->VDisplay;

	pDGAMode->bytesPerScanline = pVBox->maxBytesPerScanline;
	pDGAMode->imageWidth = pMode->HDisplay;
	pDGAMode->imageHeight =  pMode->VDisplay;
	pDGAMode->pixmapWidth = pDGAMode->imageWidth;
	pDGAMode->pixmapHeight = pDGAMode->imageHeight;
	pDGAMode->maxViewportX = pScrn->virtualX -
				    pDGAMode->viewportWidth;
	pDGAMode->maxViewportY = pScrn->virtualY -
				    pDGAMode->viewportHeight;

	pDGAMode->address = pVBox->base;

	pMode = pMode->next;
    } while (pMode != pScrn->modes);
}

static Bool
VBOXDGAInit(ScrnInfoPtr pScrn, ScreenPtr pScreen)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);

    if (!pVBox->nDGAMode)
	VBOXDGAAddModes(pScrn);

    return (DGAInit(pScreen, &VBOXDGAFunctions,
	    pVBox->pDGAMode, pVBox->nDGAMode));
}
