/** @file
 *
 * VirtualBox X11 Additions graphics driver
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
 *
 * $XFree86: xc/programs/Xserver/hw/xfree86/drivers/vesa/vesa.h,v 1.9 2001/05/04 19:05:49 dawes Exp $
 */

#ifndef _VBOXVIDEO_H_
#define _VBOXVIDEO_H_

#include <VBox/VBoxVideoGuest.h>
#include <VBox/VBoxVideo.h>
#include <iprt/asm-math.h>

#ifdef DEBUG

#define TRACE_ENTRY() \
do { \
    xf86Msg(X_INFO, __PRETTY_FUNCTION__); \
    xf86Msg(X_INFO, ": entering\n"); \
} while(0)
#define TRACE_EXIT() \
do { \
    xf86Msg(X_INFO, __PRETTY_FUNCTION__); \
    xf86Msg(X_INFO, ": leaving\n"); \
} while(0)
#define TRACE_LOG(...) \
do { \
    xf86Msg(X_INFO, __PRETTY_FUNCTION__); \
    xf86Msg(X_INFO, __VA_ARGS__); \
} while(0)
# define TRACE_LINE() do \
{ \
    ErrorF ("%s: line %d\n", __FUNCTION__, __LINE__); \
    } while(0)
# define XF86ASSERT(expr, out) \
if (!(expr)) \
{ \
    ErrorF ("\nAssertion failed!\n\n"); \
    ErrorF ("%s\n", #expr); \
    ErrorF ("at %s (%s:%d)\n", __PRETTY_FUNCTION__, __FILE__, __LINE__); \
    ErrorF out; \
    FatalError("Aborting"); \
}
#else  /* !DEBUG */

#define TRACE_ENTRY()         do { } while (0)
#define TRACE_EXIT()          do { } while (0)
#define TRACE_LOG(...)        do { } while (0)
#define XF86ASSERT(expr, out) do { } while (0)

#endif  /* !DEBUG */

#define BOOL_STR(a) ((a) ? "TRUE" : "FALSE")

#include <VBox/Hardware/VBoxVideoVBE.h>
#include <VBox/VMMDev.h>

#include "xf86str.h"
#include "xf86Cursor.h"

#define VBOX_VERSION		4000  /* Why? */
#define VBOX_NAME		      "VBoxVideo"
#define VBOX_DRIVER_NAME	  "vboxvideo"

#ifdef VBOX_DRI
/* DRI support */
#define _XF86DRI_SERVER_
/* Hack to work around a libdrm header which is broken on Solaris */
#define u_int64_t uint64_t
/* Get rid of a warning due to a broken header file */
enum drm_bo_type { DRM_BO_TYPE };
#include "dri.h"
#undef u_int64_t
#include "sarea.h"
#include "GL/glxint.h"

/* For some reason this is not in the header files. */
extern void GlxSetVisualConfigs(int nconfigs, __GLXvisualConfig *configs,
                                void **configprivs);
#endif

#define VBOX_VIDEO_MAJOR  1
#define VBOX_VIDEO_MINOR  0
#define VBOX_DRM_DRIVER_NAME  "vboxvideo"  /* For now, as this driver is basically a stub. */
#define VBOX_DRI_DRIVER_NAME  "vboxvideo"  /* For starters. */
#define VBOX_MAX_DRAWABLES    256          /* At random. */

#define VBOXPTR(p) ((VBOXPtr)((p)->driverPrivate))

/*XXX*/

typedef struct VBOXRec
{
    EntityInfoPtr pEnt;
#ifdef PCIACCESS
    struct pci_device *pciInfo;
    struct pci_device *vmmDevInfo;
#else
    pciVideoPtr pciInfo;
    PCITAG pciTag;
#endif
    void *base;
    /** The amount of VRAM available for use as a framebuffer */
    unsigned long cbFBMax;
    /** The size of the framebuffer and the VBVA buffers at the end of it. */
    unsigned long cbView;
    /** The current line size in bytes */
    uint32_t cbLine;
    /** Whether the pre-X-server mode was a VBE mode */
    bool fSavedVBEMode;
    /** Paramters of the saved pre-X-server VBE mode, invalid if there is none
     */
    uint16_t cSavedWidth, cSavedHeight, cSavedPitch, cSavedBPP, fSavedFlags;
    CloseScreenProcPtr CloseScreen;
    /** Default X server procedure for enabling and disabling framebuffer access */
    xf86EnableDisableFBAccessProc *EnableDisableFBAccess;
    OptionInfoPtr Options;
    /** @todo we never actually free this */
    xf86CursorInfoPtr pCurs;
    Bool useDevice;
    Bool forceSWCursor;
    /** Do we know that the guest can handle absolute co-ordinates? */
    Bool guestCanAbsolute;
    /** Does this host support sending graphics commands using HGSMI? */
    Bool fHaveHGSMI;
    /** Number of screens attached */
    uint32_t cScreens;
    /** Position information for each virtual screen for the purposes of
     * sending dirty rectangle information to the right one. */
    RTRECT2 aScreenLocation[VBOX_VIDEO_MAX_SCREENS];
    /** The last requested framebuffer size. */
    RTRECTSIZE FBSize;
    /** Has this screen been disabled by the guest? */
    Bool afDisabled[VBOX_VIDEO_MAX_SCREENS];
#ifdef VBOXVIDEO_13
    /** The virtual crtcs */
    struct _xf86Crtc *paCrtcs[VBOX_VIDEO_MAX_SCREENS];
    struct _xf86Output *paOutputs[VBOX_VIDEO_MAX_SCREENS];
#endif
    /** Offsets of VBVA buffers in video RAM */
    uint32_t aoffVBVABuffer[VBOX_VIDEO_MAX_SCREENS];
    /** Context information about the VBVA buffers for each screen */
    struct VBVABUFFERCONTEXT aVbvaCtx[VBOX_VIDEO_MAX_SCREENS];
    /** The current preferred resolution for the screen */
    RTRECTSIZE aPreferredSize[VBOX_VIDEO_MAX_SCREENS];
    /** HGSMI guest heap context */
    HGSMIGUESTCOMMANDCONTEXT guestCtx;
    /** Unrestricted horizontal resolution flag. */
    Bool fAnyX;
#ifdef VBOX_DRI
    Bool useDRI;
    int cVisualConfigs;
    __GLXvisualConfig *pVisualConfigs;
    DRIInfoRec *pDRIInfo;
    int drmFD;
#endif
} VBOXRec, *VBOXPtr;

extern Bool vbox_init(int scrnIndex, VBOXPtr pVBox);
extern Bool vbox_cursor_init (ScreenPtr pScreen);
extern Bool vbox_open (ScrnInfoPtr pScrn, ScreenPtr pScreen, VBOXPtr pVBox);
extern void vbox_close (ScrnInfoPtr pScrn, VBOXPtr pVBox);
extern Bool vbox_device_available(VBOXPtr pVBox);

extern Bool vboxEnableVbva(ScrnInfoPtr pScrn);
extern void vboxDisableVbva(ScrnInfoPtr pScrn);

extern Bool vboxEnableGraphicsCap(VBOXPtr pVBox);
extern Bool vboxDisableGraphicsCap(VBOXPtr pVBox);
extern Bool vboxGuestIsSeamless(ScrnInfoPtr pScrn);

extern Bool vboxGetDisplayChangeRequest(ScrnInfoPtr pScrn, uint32_t *pcx,
                                        uint32_t *pcy, uint32_t *pcBits,
                                        uint32_t *piDisplay);
extern Bool vboxHostLikesVideoMode(ScrnInfoPtr pScrn, uint32_t cx, uint32_t cy, uint32_t cBits);
extern Bool vboxSaveVideoMode(ScrnInfoPtr pScrn, uint32_t cx, uint32_t cy, uint32_t cBits);
extern Bool vboxRetrieveVideoMode(ScrnInfoPtr pScrn, uint32_t *pcx, uint32_t *pcy, uint32_t *pcBits);
extern unsigned vboxNextStandardMode(ScrnInfoPtr pScrn, unsigned cIndex,
                                     uint32_t *pcx, uint32_t *pcy,
                                     uint32_t *pcBits);
extern void vboxGetPreferredMode(ScrnInfoPtr pScrn, uint32_t iScreen,
                                 uint32_t *pcx, uint32_t *pcy,
                                 uint32_t *pcBits);
extern void vboxWriteHostModes(ScrnInfoPtr pScrn, DisplayModePtr pCurrent);
extern void vboxAddModes(ScrnInfoPtr pScrn, uint32_t cxInit,
                         uint32_t cyInit);

/* DRI stuff */
extern Bool VBOXDRIScreenInit(ScrnInfoPtr pScrn, ScreenPtr pScreen,
                              VBOXPtr pVBox);
extern Bool VBOXDRIFinishScreenInit(ScreenPtr pScreen);
extern void VBOXDRIUpdateStride(ScrnInfoPtr pScrn, VBOXPtr pVBox);
extern void VBOXDRICloseScreen(ScreenPtr pScreen, VBOXPtr pVBox);

/* EDID generation */
#ifdef VBOXVIDEO_13
extern Bool VBOXEDIDSet(struct _xf86Output *output, DisplayModePtr pmode);
#endif

/* Utilities */

static inline VBOXPtr VBOXGetRec(ScrnInfoPtr pScrn)
{
    return ((VBOXPtr)pScrn->driverPrivate);
}

/** Calculate the BPP from the screen depth */
static inline uint16_t vboxBPP(ScrnInfoPtr pScrn)
{
    return pScrn->depth == 24 ? 32 : 16;
}

/** Calculate the scan line length for a display width */
static inline int32_t vboxLineLength(ScrnInfoPtr pScrn, int32_t cDisplayWidth)
{
    uint64_t cbLine = ((uint64_t)cDisplayWidth * vboxBPP(pScrn) / 8 + 3) & ~3;
    return cbLine < INT32_MAX ? cbLine : INT32_MAX;
}

/** Calculate the display pitch from the scan line length */
static inline int32_t vboxDisplayPitch(ScrnInfoPtr pScrn, int32_t cbLine)
{
    /* take care to reference __udivdi3! */
    return ASMDivU64ByU32RetU32((uint64_t)cbLine * 8, vboxBPP(pScrn));
}

extern void vboxClearVRAM(ScrnInfoPtr pScrn, int32_t cNewX, int32_t cNewY);
extern Bool VBOXSetMode(ScrnInfoPtr pScrn, unsigned cDisplay, unsigned cWidth,
                        unsigned cHeight, int x, int y);
extern Bool VBOXAdjustScreenPixmap(ScrnInfoPtr pScrn, int width, int height);

#endif /* _VBOXVIDEO_H_ */

