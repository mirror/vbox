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

#ifdef XORG_7X
# include "xorg-server.h"
# include <string.h>
#endif
#include "vboxvideo.h"
#include "version-generated.h"
#include "product-generated.h"
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

/* VGA hardware functions for setting and restoring text mode */
#include "vgaHW.h"

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
static Bool VBOXSaveRestore(ScrnInfoPtr pScrn,
                            vbeSaveRestoreFunction function);
static bool VBOXAdjustScreenPixmap(ScrnInfoPtr pScrn, DisplayModePtr pMode);

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

/*
 * This contains the functions needed by the server after loading the
 * driver module.  It must be supplied, and gets added the driver list by
 * the Module Setup funtion in the dynamic case.  In the static case a
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
    OPTION_SHADOW_FB,
    OPTION_DFLT_REFRESH,
    OPTION_MODESET_CLEAR_SCREEN
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

static const char *vgahwSymbols[] = {
    "vgaHWGetHWRec",
    "vgaHWHandleColormaps",
    "vgaHWFreeHWRec",
    "vgaHWMapMem",
    "vgaHWUnmapMem",
    "vgaHWSaveFonts",
    "vgaHWRestoreFonts",
    "vgaHWGetIndex",
    "vgaHWSaveScreen",
    "vgaHWDPMSSet",
    NULL
};

#ifdef XFree86LOADER
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
    ABI_CLASS_VIDEODRV,	        /* This is a video driver */
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
                          vbeSymbols,
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

static VBOXPtr
VBOXGetRec(ScrnInfoPtr pScrn)
{
    if (!pScrn->driverPrivate)
    {
        pScrn->driverPrivate = xcalloc(sizeof(VBOXRec), 1);
#if 0
        ((VBOXPtr)pScrn->driverPrivate)->vbox_fd = -1;
#endif
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
 * This function is called once, at the start of the first server generation to
 * do a minimal probe for supported hardware.
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
        VBOXPtr pVBox = VBOXGetRec(pScrn);

        pScrn->driverVersion = VBOX_VERSION;
        pScrn->driverName    = VBOX_DRIVER_NAME;
        pScrn->name          = VBOX_NAME;
        pScrn->Probe         = NULL;
        pScrn->PreInit       = VBOXPreInit;
        pScrn->ScreenInit    = VBOXScreenInit;
        pScrn->SwitchMode    = VBOXSwitchMode;
        /* pScrn->ValidMode     = VBOXValidMode; */
        pScrn->AdjustFrame   = VBOXAdjustFrame;
        pScrn->EnterVT       = VBOXEnterVT;
        pScrn->LeaveVT       = VBOXLeaveVT;
        pScrn->FreeScreen    = VBOXFreeScreen;

        pVBox->pciInfo = dev;
    }

    TRACE_LOG("returning %s\n", BOOL_STR(pScrn != NULL));
    return (pScrn != NULL);
}
#else /* !PCIACCESS */
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
			        pScrn->name	         = VBOX_NAME;
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
#endif /* !PCIACCESS */

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

    pVBox->accessEnabled = enable;
    pVBox->EnableDisableFBAccess(scrnIndex, enable);
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
    int i;
    DisplayModePtr pMode;
    enum { MODE_MIN_SIZE = 64 };

    TRACE_ENTRY();
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

    /* The ramdac module is needed for the hardware cursor. */
    if (!xf86LoadSubModule(pScrn, "ramdac"))
        return FALSE;

    /* We need the vbe module because we use VBE code to save and restore
       text mode, in order to keep our code simple. */
    if (!xf86LoadSubModule(pScrn, "vbe"))
        return (FALSE);

    /* The framebuffer module. */
    if (xf86LoadSubModule(pScrn, "fb") == NULL)
        return (FALSE);

    if (!xf86LoadSubModule(pScrn, "shadowfb"))
        return FALSE;

    if (!xf86LoadSubModule(pScrn, "vgahw"))
        return FALSE;

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
    pScrn->clockRanges = xnfcalloc(sizeof(ClockRange), 1);
    pScrn->clockRanges->minClock = 1000;
    pScrn->clockRanges->maxClock = 1000000000;
    pScrn->clockRanges->clockIndex = -1;
    pScrn->clockRanges->ClockMulFactor = 1;
    pScrn->clockRanges->ClockDivFactor = 1;

    /* Determine the preferred size and colour depth and setup video modes */
    {
        uint32_t cx = 0, cy = 0, cBits = 0;

        vboxGetPreferredMode(pScrn, &cx, &cy, &cBits);
        /* We only support 16 and 24 bits depth (i.e. 16 and 32bpp) */
        if (cBits != 16)
            cBits = 24;
        if (!xf86SetDepthBpp(pScrn, cBits, 0, 0, Support32bppFb))
            return FALSE;
        vboxAddModes(pScrn, cx, cy);
    }
    if (pScrn->bitsPerPixel != 32 && pScrn->bitsPerPixel != 16)
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

    /* We don't validate with xf86ValidateModes and xf86PruneModes as we
     * already know what we like and what we don't. */

    pScrn->currentMode = pScrn->modes;

    /* Set the right virtual resolution. */
    pScrn->virtualX = pScrn->currentMode->HDisplay;
    pScrn->virtualY = pScrn->currentMode->VDisplay;

    pScrn->displayWidth = pScrn->virtualX;

    xf86PrintModes(pScrn);

    /* Set the DPI.  Perhaps we should read this from the host? */
    xf86SetDpi(pScrn, 96, 96);

    /* options */
    xf86CollectOptions(pScrn, NULL);
    if (!(pVBox->Options = xalloc(sizeof(VBOXOptions))))
        return FALSE;
    memcpy(pVBox->Options, VBOXOptions, sizeof(VBOXOptions));
    xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, pVBox->Options);

    /* Framebuffer-related setup */
    pScrn->bitmapBitOrder = BITMAP_BIT_ORDER;

    /* VGA hardware initialisation */
    if (!vgaHWGetHWRec(pScrn))
        return FALSE;

    TRACE_EXIT();
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
 * End QUOTE.
 */
static Bool
VBOXScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    VisualPtr visual;
    unsigned flags;

    TRACE_ENTRY();
    /* We make use of the X11 VBE code to save and restore text mode, in
       order to keep our code simple. */
    if ((pVBox->pVbe = VBEExtendedInit(NULL, pVBox->pEnt->index,
                                       SET_BIOS_SCRATCH
                                       | RESTORE_BIOS_SCRATCH)) == NULL)
        return (FALSE);

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

    /* set the viewport */
    VBOXAdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);

    /* Blank the screen for aesthetic reasons. */
    VBOXSaveScreen(pScreen, SCREEN_SAVER_ON);

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

    xf86SetBlackWhitePixels(pScreen);
    miInitializeBackingStore(pScreen);
    xf86SetBackingStore(pScreen);

    /* We need to keep track of whether we are currently switched to a virtual
     * terminal to know whether a mode set operation is currently safe to do.
     */
    pVBox->vtSwitch = FALSE;
    /* software cursor */
    miDCInitialize(pScreen, xf86GetPointerScreenFuncs());

    /* colourmap code - apparently, we need this even in Truecolour */
    if (!miCreateDefColormap(pScreen))
	return (FALSE);

    flags = CMAP_RELOAD_ON_MODE_SWITCH;

    if(!vgaHWHandleColormaps(pScreen))
         return (FALSE);

    /* Hook our observer function ito the chain which is called when
     * framebuffer access is enabled or disabled in the server, and
     * assume an initial state of enabled. */
    pVBox->accessEnabled = TRUE;
    pVBox->EnableDisableFBAccess = pScrn->EnableDisableFBAccess;
    pScrn->EnableDisableFBAccess = vboxEnableDisableFBAccess;

    pVBox->CloseScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = VBOXCloseScreen;
    pScreen->SaveScreen = VBOXSaveScreen;

    xf86DPMSInit(pScreen, VBOXDisplayPowerManagementSet, 0);

    /* Report any unused options (only for the first generation) */
    if (serverGeneration == 1)
        xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);

    /* set first video mode */
    if (!VBOXSetMode(pScrn, pScrn->currentMode))
        return FALSE;
    /* And make sure that a non-current dynamic mode is at the front of the
     * list */
    vboxWriteHostModes(pScrn, pScrn->currentMode);

    if (vbox_open (pScrn, pScreen, pVBox)) {
        if (vbox_cursor_init(pScreen) != TRUE)
            xf86DrvMsg(scrnIndex, X_ERROR,
                       "Unable to start the VirtualBox mouse pointer integration with the host system.\n");
        if (vboxEnableVbva(pScrn) == TRUE)
            xf86DrvMsg(scrnIndex, X_INFO,
                      "The VBox video extensions are now enabled.\n");
        vboxEnableGraphicsCap(pVBox);
    }
    TRACE_EXIT();
    return (TRUE);
}

static Bool
VBOXEnterVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    VBOXPtr pVBox = VBOXGetRec(pScrn);

    TRACE_ENTRY();
    pVBox->vtSwitch = FALSE;
    if (!VBOXSetMode(pScrn, pScrn->currentMode))
        return FALSE;
    VBOXAdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);
    if (pVBox->useVbva == TRUE)
        vboxEnableVbva(pScrn);
    return TRUE;
}

static void
VBOXLeaveVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    VBOXPtr pVBox = VBOXGetRec(pScrn);

    TRACE_ENTRY();
    VBOXSaveRestore(pScrn, MODE_RESTORE);
    if (pVBox->useVbva == TRUE)
        vboxDisableVbva(pScrn);
    vboxDisableGraphicsCap(pVBox);
    pVBox->vtSwitch = TRUE;
}

static Bool
VBOXCloseScreen(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    VBOXPtr pVBox = VBOXGetRec(pScrn);

    if (pVBox->useVbva == TRUE)
        vboxDisableVbva(pScrn);
    vboxDisableGraphicsCap(pVBox);
    if (pScrn->vtSema) {
        VBOXSaveRestore(xf86Screens[scrnIndex], MODE_RESTORE);
        if (pVBox->savedPal)
            VBESetGetPaletteData(pVBox->pVbe, TRUE, 0, 256,
                                 pVBox->savedPal, FALSE, TRUE);
        VBOXUnmapVidMem(pScrn);
    }
    pScrn->vtSema = FALSE;
    
    /* Destroy the VGA hardware record */
    vgaHWFreeHWRec(pScrn);

    pScreen->CloseScreen = pVBox->CloseScreen;
    return pScreen->CloseScreen(scrnIndex, pScreen);
}

static Bool
VBOXSwitchMode(int scrnIndex, DisplayModePtr pMode, int flags)
{
    ScrnInfoPtr pScrn;
    VBOXPtr pVBox;
    Bool rc = TRUE;

    pScrn = xf86Screens[scrnIndex];  /* Why does X have three ways of refering to the screen? */
    pVBox = VBOXGetRec(pScrn);
    if (pVBox->useVbva)
        if (!vboxDisableVbva(pScrn))  /* This would be bad. */
            rc = FALSE;
    /* We want to disable access to the framebuffer before switching mode.
     * After doing the switch, we allow access if it was allowed before. */
    if (pVBox->accessEnabled)
        pVBox->EnableDisableFBAccess(scrnIndex, FALSE);
    if (rc && !VBOXSetMode(pScrn, pMode))
        rc = FALSE;
    if (rc && !VBOXAdjustScreenPixmap(pScrn, pMode))
        rc = FALSE;
    if (rc && !vboxGuestIsSeamless(pScrn))
        vboxSaveVideoMode(pScrn, pMode->HDisplay, pMode->VDisplay,
                          pScrn->bitsPerPixel);
    if (rc)
    {
        vboxWriteHostModes(pScrn, pMode);
        xf86PrintModes(pScrn);
    }
    if (pVBox->accessEnabled)
        pVBox->EnableDisableFBAccess(scrnIndex, TRUE);
    if (pVBox->useVbva)
        if (!vboxEnableVbva(pScrn))  /* Bad but not fatal */
            pVBox->useVbva = FALSE;
    return rc;
}

/* Set a graphics mode */
static Bool
VBOXSetMode(ScrnInfoPtr pScrn, DisplayModePtr pMode)
{
    TRACE_ENTRY();
    int bpp = pScrn->depth == 24 ? 32 : 16;
    VBOXPtr pVBox = VBOXGetRec(pScrn);

    /* Don't fiddle with the hardware if we are switched
     * to a virtual terminal. */
    if (pVBox->vtSwitch == TRUE)
        return TRUE;
    if (pScrn->virtualX * pScrn->virtualY * bpp / 8
        >= pScrn->videoRam * 1024)
    {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "Unable to set up a virtual screen size of %dx%d with %d Kb of video memory.  Please increase the video memory size.\n",
                   pScrn->virtualX, pScrn->virtualY, pScrn->videoRam);
        return FALSE;
    }

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
    outw(VBE_DISPI_IOPORT_DATA, pMode->HDisplay);
    outw(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_YRES);
    outw(VBE_DISPI_IOPORT_DATA, pMode->VDisplay);
    /* Enable linear framebuffer mode. */
    outw(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_ENABLE);
    outw(VBE_DISPI_IOPORT_DATA,
         VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
    /* Set the virtual resolution.  We are still using VESA to control
       the virtual offset. */
    outw(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_VIRT_WIDTH);
    outw(VBE_DISPI_IOPORT_DATA, pMode->HDisplay);
    pVBox->cLastWidth = pMode->HDisplay;
    pVBox->cLastHeight = pMode->VDisplay;
    vboxEnableGraphicsCap(pVBox);
    TRACE_EXIT();
    return (TRUE);
}

static bool VBOXAdjustScreenPixmap(ScrnInfoPtr pScrn, DisplayModePtr pMode)
{
    ScreenPtr pScreen = pScrn->pScreen;
    PixmapPtr pPixmap = pScreen->GetScreenPixmap(pScreen);
    VBOXPtr pVBox = VBOXGetRec(pScrn);
    int bpp = pScrn->depth == 24 ? 32 : 16;
    if (!pPixmap) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "Failed to get the screen pixmap.\n");
        return FALSE;
    }
    pScreen->ModifyPixmapHeader(pPixmap, pMode->HDisplay, pMode->VDisplay,
                                pScrn->depth, bpp, pMode->HDisplay * bpp / 8,
                                pVBox->base);
    pScrn->EnableDisableFBAccess(pScrn->scrnIndex, FALSE);
    pScrn->EnableDisableFBAccess(pScrn->scrnIndex, TRUE);
    return TRUE;
}

static void
VBOXAdjustFrame(int scrnIndex, int x, int y, int flags)
{
    VBOXPtr pVBox = VBOXGetRec(xf86Screens[scrnIndex]);
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];

    /* Don't fiddle with the hardware if we are switched
     * to a virtual terminal. */
    if (pVBox->vtSwitch == TRUE)
        return;
    pVBox->viewportX = x;
    pVBox->viewportY = y;
    /* If VBVA is enabled the graphics card will not notice the change. */
    if (pVBox->useVbva == TRUE)
        vboxDisableVbva(pScrn);
    VBESetDisplayStart(pVBox->pVbe, x, y, TRUE);
    if (pVBox->useVbva == TRUE)
        vboxEnableVbva(pScrn);
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

#ifdef PCIACCESS
    (void) pci_device_map_range(pVBox->pciInfo,
                                pScrn->memPhysBase,
                                pVBox->mapSize,
                                PCI_DEV_MAP_FLAG_WRITABLE,
                                & pVBox->base);
#else
    pVBox->base = xf86MapPciMem(pScrn->scrnIndex,
                                VIDMEM_FRAMEBUFFER,
                                pVBox->pciTag, pVBox->mapPhys,
                                (unsigned) pVBox->mapSize);
#endif
    if (pVBox->base)
    {
        /* We need this for saving/restoring textmode */
        VGAHWPTR(pScrn)->IOBase = pScrn->domainIOBase;
        return vgaHWMapMem(pScrn);
    }
    else
        return FALSE;
}

static void
VBOXUnmapVidMem(ScrnInfoPtr pScrn)
{
    VBOXPtr pVBox = VBOXGetRec(pScrn);

    if (pVBox->base == NULL)
        return;

#ifdef PCIACCESS
    (void) pci_device_unmap_range(pVBox->pciInfo,
                                  pVBox->base,
                                  pVBox->mapSize);
#else
    xf86UnMapVidMem(pScrn->scrnIndex, pVBox->base,
                    (unsigned) pVBox->mapSize);
#endif
    vgaHWUnmapMem(pScrn);
    pVBox->base = NULL;
}

static Bool
VBOXSaveScreen(ScreenPtr pScreen, int mode)
{
    return vgaHWSaveScreen(pScreen, mode);
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
        vgaHWSaveFonts(pScrn, &pVBox->vgaRegs);

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
            vgaHWRestoreFonts(pScrn, &pVBox->vgaRegs);
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
    vgaHWDPMSSet(pScrn, mode, flags);
}
